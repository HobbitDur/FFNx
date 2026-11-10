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

// Effect 84: Angel Wing (kernel.bin magic Angel Wing +4 = 84: Rinoa's limit break, the transformation; MAG_084_ANGEL_WING*,
// texture file 0x1578430 "mag083.tim"): a copy of the actor/heal effect library (see act_engine.h) built like MiniMog (mag096_minimog.cpp:
// master, the director / cut machinery of Tonberry / Doom, the prim-model plays and renderers of Doom / Siren / Angelo, the actor
// systems, the reveal ("dark") model renderer of LV?Death / Doom) plus the module's own parts. Module 0x78D790..0x7976A0 (180
// functions: 77 served by engine ports, 62 the relocated text of a ported library function (Doom 8, Siren 95, MiniMog 96,
// Ultimecia 82 / 83, Tonberry 90, Angelo 86 / 87 / 92 / 94, Invincible Moon 88, Boko 97 / 99, Drink Magic 37, LV5 Death 345,
// The End 85), 39 hand-ported (3 of them Doom's text with the module's changes), the loader and the setup not ported).
//
// Setup MAG_084_ANGEL_WING_Init 0x78D7B0 (runs once, not ported; file loader 0x78D790 = the texture file 0x1578430 ->
// [0x25C2350], TIM uploaded unless the cast context flag +1 bit 0; packet arenas at file + 0 / + 0xC000 by tick parity
// (0x25C9234 / 0x25C9230 -> cursor 0x25C2354), the file arena cursor 0x25C9F38 = file + 0x18000) creates the root queue
// 0x25C2588 with the master and eight task pools.
//   Master (0x78D940 = MiniMog's master 0x731F70 with 11 states) - camera copy 0x2793E58 (pointers 0x25C9F08 / 0x25C24B8),
//     packet arena by tick parity, target bone follow, states: 0x78DAF0 carves the two pools of the file arena (0x1B0 bytes
//     of emitter records 0x25C9EEC + 0x6900 bytes of actor records 0x25C9F30), 0x78DB70 starts 0x78DBA0 (a_73A1A0), then the
//     library's finish states; executes the eight module queues.
//   0x78DBA0 / 0x78DC20 - the director block [0x15720A8] = 0x25C9D28 (0x78DC50: caster / target positions, facing) and the
//     DIRECTOR 0x78DCE0 (16 states): 0x78DE00 the texture 0x17243F8 (exe data) + BS_Camera_ArmReturnAfterEffect + the camera
//     task 0x78DE40 (camera scripts 0x1575DE0 / 0x1575E28 / 0x1575F90 through the library stepper on the camera block
//     [0x1572BAC] = 0x25C9C38); cut 1 hides every other battle entity (0x78E8F0: flag word bit 2 of the draw entries
//     0x1D972C0 + 0x9C k, the old words kept in the director block +0x20), cut 4 shows them again (0x78EA70); cuts 1..5.
//   Cuts (0x78DDB0 -> MAG_084_SpawnAngelWingsModel 0x78EB50): 1 - the MODEL CONTAINER 0x7961E0 (Rinoa's angel model 0x1572BB0,
//     animation 3; its set-up 0x796BF0 SWAPS THE CASTER'S MODEL: BS_BindEntityModelFromComData(caster, 0x25C9F10, 0x1576020) +
//     QueueChainTransformation(caster, 1)), drawn first by the REVEAL renderer 0x796720 (vertices lit by their distance to the
//     reveal centre, radius 0x25C23B4 growing by 0x20 a tick to 0x500, centre height 0x25C23B6 = the model's point 0xF1;
//     sound 0x15720AC), from cut 2 by the creature draw a_746C10, faded out at cut 4; its prim play 0x796290 (layout 0x1720570)
//     on frames 0x32 / 0x64; the prim play 0x78ECD0 (layout 0x171F2B0, from frame 0x18 to cut 3); glow A 0x796FD0 (model
//     0x15720B0 at Rinoa's point 0xF1, camera-facing) and glow B 0x7972B0 (model 0x1572810 at the camera block's height,
//     hidden from cut 3 to cut 4); 2 - the feather actor system 0x7913C0 (descriptor 0x171DE1C: particles born at random
//     bones of the angel model, 0x794B50 / table 0x1572B90); 3 - the prim play 0x791260 (layout 0x17217A8, to cut 4) and a
//     second feather actor system. Cut 5: the done word +0x48 of the director block.
//   The prim plays draw through the object callback 0x78EEE0 (Doom's 0x8C5B10 shape) and the module renderer 0x78F1B0
//   (Siren's 0x73C790 family: lists 0x78F2E0 / 0x78F500 / 0x78F790 / 0x78FC10 / 0x7906C0 / 0x790BC0); the actor systems (0x791450
//   = a_73F990 family) are the library's, drawing into the module arena.
// ENTITY / CAMERA / ENGINE WRITES: the caster's model (BS_BindEntityModelFromComData, QueueChainTransformation); the other battle
// entities' draw flag bit 2 (hidden during cuts 1..3); the camera scripts (library stepper -> battle camera words) and
// BS_Camera_ArmReturnAfterEffect; TIM uploads; the sound 0x15720AC (BdPlaySE); the target bone follow
// (Effect_UpdateTargetPosFromBones); action results through the library (0x797540 -> 0x506690).
// Module globals: 0x25C2350..0x25C9F70 (texture file 0x25C2350, cursors, queues and pools, the camera block 0x25C9C38, the
// director block 0x25C9D28, the reveal draw context 0x25C2360); data: 0x15720A8.. (director / camera pointers, models, sound),
// the model container 0x1572BB0 (its skeleton block 0x1572BC8 is rewritten), layouts 0x171F2B0 / 0x17217A8 / 0x1720570, actor
// data 0x171DE1C, the camera scripts 0x1575DE0.., the bone table 0x1572B90.

#include "act_engine.h"
#include <stddef.h>

namespace ff8fx
{
namespace act
{
namespace angelwing
{
	// ---- generated (desc84.py): the module descriptor of this copy of the library ----
	static const Mod MOD_084 = { "angelwing", 84, 0x78D790, 0x7976A0,
		{ 0x0, 0x0, 0x15720A8, 0x1572BAC, 0x1575FD8, 0x25C24B4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25C2350, 0x0, 0x0, 0x0, 0x25C24B8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25C8E20, 0x0, 0x25C90C0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25C9F3C },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7922A0, 0x7924B0, 0x792730, 0x792970, 0x792C10, 0x792E40, 0x793100, 0x793360, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78DBA0, 0x78DC20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78DF30, 0x78DF50, 0x78E240, 0x78E590, 0x0, 0x0, 0x0, 0x78E870, 0x78E830, 0x78E8D0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x791600, 0x7916E0, 0x791880, 0x791950, 0x791A70, 0x791CD0, 0x0, 0x0, 0x793EB0, 0x793F00, 0x0, 0x0, 0x7947F0, 0x0, 0x0, 0x0, 0x795220, 0x795250, 0x795330, 0x795450, 0x7955A0, 0x0, 0x795C60, 0x795E40, 0x795F80, 0x0, 0x0, 0x797540, 0x797580, 0x7975A0, 0x7975C0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x15720A8;        // pointer to the director block 0x25C9D28 (cut +0x40/+0x42/+0x44, frame +0x46)
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t SPRITE_CURSOR = 0x25C2354;  // module packet cursor (tick parity: file + 0 / + 0xC000)
	static const uint32_t PACKET_CURSOR = SPRITE_CURSOR;
	static const uint32_t FA_CURSOR = 0x1D8E054;      // frame arena packet cursor
	static const uint32_t SCRATCH_SP = 0x25C9F08;     // module scratch stack pointer (the camera copy 0x2793E58)
	static const uint32_t ACTOR_STATE = 0x25C9F3C;    // current actor system state block (pool header)

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
		// (Angel Wing)
		static inline uint32_t BS_Camera_ArmReturnAfterEffect() { return fn<uint32_t (__cdecl *)()>(0x50A730)(); }
		static inline uint32_t BS_BindEntityModelFromComData(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x5022C0)(a1, a2, a3); }
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
	static inline uint32_t t1_data() { return MEM<uint32_t>(G(G_1533010)); }
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

	// (Angel Wing) helpers of the Ultimecia / Angelo / LV?Death texts
	static const uint32_t REAPER_CTX = 0x25C2360;      // draw context of the reveal model draw (the reaper's dark draw context in 74 / 345)
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; } // battle entity record of a slot
	static inline void ak_cam_chain(uint32_t a1, uint32_t script, uint32_t from)
	{
		a_73AB20(script, from, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
	}

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl te_78D940(uint32_t a1);
	uint32_t __cdecl te_78DAF0(uint32_t a1);
	uint32_t __cdecl te_78DB30();
	uint32_t __cdecl te_78DC20(uint32_t a1);
	uint32_t __cdecl te_78DC50(uint32_t a1);
	uint32_t __cdecl te_78DCE0(uint32_t a1);
	uint32_t __cdecl te_78DDB0(uint32_t a1);
	uint32_t __cdecl te_78DE00(uint32_t a1);
	uint32_t __cdecl te_78DE40(uint32_t a1);
	uint32_t __cdecl te_78DED0(uint32_t a1);
	uint32_t __cdecl te_78DF00(uint32_t a1);
	uint32_t __cdecl te_78E700(uint32_t a1);
	uint32_t __cdecl te_78E730(uint32_t a1);
	uint32_t __cdecl te_78E760(uint32_t a1);
	uint32_t __cdecl te_78E790(uint32_t a1);
	uint32_t __cdecl te_78E7F0(uint32_t a1);
	uint32_t __cdecl te_78E8A0(uint32_t a1);
	uint32_t __cdecl te_78E8F0(uint32_t a1);
	uint32_t __cdecl te_78E940(uint32_t a1);
	uint32_t __cdecl te_78E9A0(uint32_t a1);
	uint32_t __cdecl te_78E9E0(uint32_t a1);
	uint32_t __cdecl te_78EA00(uint32_t a1);
	uint32_t __cdecl te_78EA20(uint32_t a1);
	uint32_t __cdecl te_78EA40(uint32_t a1);
	uint32_t __cdecl te_78EA70(uint32_t a1);
	uint32_t __cdecl te_78EAC0(uint32_t a1);
	uint32_t __cdecl te_78EAE0(uint32_t a1);
	uint32_t __cdecl te_78EB50(uint32_t a1);
	uint32_t __cdecl te_78EC80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl te_78ED30(uint32_t a1);
	uint32_t __cdecl te_78EDB0(uint32_t a1);
	uint32_t __cdecl te_78EEE0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl te_78F1B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_78F2E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_78F500(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_78F790(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_78FC10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_7906C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_790BC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_791210(uint32_t a1);
	uint32_t __cdecl te_7912C0(uint32_t a1);
	uint32_t __cdecl te_791350(uint32_t a1);
	uint32_t __cdecl te_791420(uint32_t a1);
	uint32_t __cdecl te_791600(uint32_t a1);
	uint32_t __cdecl te_791880(void);
	uint32_t __cdecl te_791F30(uint32_t a1);
	uint32_t __cdecl te_791FC0(void);
	uint32_t __cdecl te_792020(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_7922A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_7924B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_792C10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_792E40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_793100(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_793360(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_793640(void);
	uint32_t __cdecl te_793700(uint32_t a1);
	uint32_t __cdecl te_793780(void);
	uint32_t __cdecl te_7937C0(uint32_t a1);
	uint32_t __cdecl te_7947F0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_794900(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_794980(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_7949B0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl te_794A50(uint32_t a1);
	uint32_t __cdecl te_794AD0(uint32_t a1);
	uint32_t __cdecl te_794B50(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl te_795380(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_7955A0(uint32_t a1);
	uint32_t __cdecl te_796090(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_796120(void);
	uint32_t __cdecl te_7961B0(uint32_t a1);
	uint32_t __cdecl te_7961E0(uint32_t a1);
	uint32_t __cdecl te_7962F0(uint32_t a1);
	uint32_t __cdecl te_796360(uint32_t a1);
	uint32_t __cdecl te_796390(uint32_t a1);
	uint32_t __cdecl te_796620(uint32_t a1);
	uint32_t __cdecl te_796720(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_796BF0(uint32_t a1);
	uint32_t __cdecl te_796C70(uint32_t a1);
	uint32_t __cdecl te_796CA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl te_796D00(uint32_t a1);
	uint32_t __cdecl te_796DB0(uint32_t a1);
	uint32_t __cdecl te_796DD0(uint32_t a1);
	uint32_t __cdecl te_796E30(uint32_t a1, uint32_t a2);
	uint32_t __cdecl te_796F50(uint32_t a1);
	uint32_t __cdecl te_796FD0(uint32_t a1);
	uint32_t __cdecl te_797080(uint32_t a1);
	uint32_t __cdecl te_797190(uint32_t a1);
	uint32_t __cdecl te_7971B0(uint32_t a1);
	uint32_t __cdecl te_7971F0(uint32_t a1);
	uint32_t __cdecl te_797220(uint32_t a1);
	uint32_t __cdecl te_797250(uint32_t a1);
	uint32_t __cdecl te_797270(uint32_t a1);
	uint32_t __cdecl te_7972B0(uint32_t a1);
	uint32_t __cdecl te_797330(uint32_t a1);
	uint32_t __cdecl te_797410(uint32_t a1);
	uint32_t __cdecl te_797430(uint32_t a1);
	uint32_t __cdecl te_797490(uint32_t a1);
	uint32_t __cdecl te_7974C0(uint32_t a1);
	uint32_t __cdecl te_7974E0(uint32_t a1);
	uint32_t __cdecl te_797500(uint32_t a1);
	uint32_t __cdecl te_797540(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag084_angel_wing_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace angelwing
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code matches a
	// Doom / Siren / MiniMog / Tonberry / Ultimecia / Angelo / Boko / Invincible Moon port up to the module
	// addresses and callees (the port's text with the module's addresses; their comments describe the source)
	// ====================================================================================

	// 0x731F70 (module 096 MAG_096_sub_731F70): MiniMog MASTER task - snapshots the camera matrix
	// into 0x2793E58, picks the double-buffered pools by tick parity, runs the 11-state master
	// state table, then executes the 8 module task queues (live task count -> node +0x5E).
	uint32_t __cdecl te_78D940(uint32_t a1)
	{
		g_mod = &MOD_084;
		// 30 fps layer: see mag084_angel_wing_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x25C9F08) = 0x2793E58;
		MEM<uint32_t>(0x25C24B8) = 0x2793E58;
		states[0] = 0x78DAD0;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x78DAE0;
		states[2] = 0x78DAF0;
		states[3] = 0x78DB70;
		states[4] = 0x7975D0;
		states[5] = 0x797610;
		states[6] = 0x797620;
		states[7] = 0x797630;
		states[8] = 0x797660;
		states[9] = 0x797670;
		states[10] = 0x797690;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x25C9234);
			const uint32_t v2 = MEM<uint32_t>(0x25C838C);
			MEM<uint32_t>(0x25C2354) = v1;
			MEM<uint32_t>(0x25C24B4) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x25C9230);
			const uint32_t v2 = MEM<uint32_t>(0x25C8388);
			MEM<uint32_t>(0x25C2354) = v1;
			MEM<uint32_t>(0x25C24B4) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(0x25C9F00) = 0;
		MEM<uint16_t>(0x25C9EE8) = 0;
		const uint32_t queues[8] = { 0x25C90C0, 0x25C2490, 0x25C9ED8, 0x25C8E20, 0x25C2598, 0x25C3938, 0x25C8378, 0x25C90B0 };
		for (int i = 0; i < 8; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		// 30 fps layer: see mag084_angel_wing_held.inc
		FX_HELD(held_note_end();)
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x78DAF0 (MAG_084_sub_78DAF0): master state 2 - once no child is alive: the two pools are carved from the file arena
	// cursor [0x25C9F38] (0x1B0 bytes at 0x25C9EEC = the emitter records, 0x6900 bytes at 0x25C9F30 = the actor records)
	// and cleared (0x78DB30), next state
	uint32_t __cdecl te_78DAF0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x25C9F38);
		MEM<uint32_t>(0x25C9EEC) = p;
		p += 0x1B0;
		MEM<uint32_t>(0x25C9F30) = p;
		p += 0x6900;
		MEM<uint32_t>(0x25C9F38) = p;
		te_78DB30();
		next_state(a1);
		return 0; // void
	}

	// 0x78DB30 (MAG_084_sub_78DB30): clears the two pools (0x1B0 / 0x6900 bytes) and the pool cursors 0x25C3930 /
	// 0x25C9EF8 / 0x25C9D24 / 0x25C2358
	uint32_t __cdecl te_78DB30()
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25C9EEC), 0x1B0);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25C9F30), 0x6900);
		MEM<uint16_t>(0x25C3930) = 0;
		MEM<uint16_t>(0x25C9EF8) = 0;
		MEM<uint16_t>(0x25C9D24) = 0;
		MEM<uint16_t>(0x25C2358) = 0;
		return 0;
	}

	// 0x8A36A0 (copy of d_8B6D10 0x8B6D10: MAG_014_sub_8B6D10): emitter state 0 - places the reaper, spawns the director
	// 0x8B6E30 (0x48 bytes, director queue), next state
	uint32_t __cdecl te_78DC20(uint32_t a1)
	{
		te_78DC50(a1);
		x::Effect_AddTaskAndInitFromCtx(0x25C2490, 0x78DCE0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8A36D0 (MAG_019_sub_8A36D0): clears the director block [0x1629FA8] (0x54 bytes), keeps the
	// positions of the caster (node +0x2C: +0x00 / +0x04) and of the target (node +0x2D: +0x08 /
	// +0x0C) and the caster's facing towards the target (CartesianToGameAngle of the x / z delta,
	// - 0x800, 12 bits) in +0x4A
	uint32_t __cdecl te_78DC50(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x15720A8), 0x54);
		const uint32_t d = MEM<uint32_t>(0x15720A8);
		const uint32_t tgt = ENT(U8(a1, 0x2D));
		U32(d, 8) = U32(tgt, 0x1C);
		U32(d, 0xC) = U32(tgt, 0x20);
		const uint32_t cst = ENT(U8(a1, 0x2C));
		U32(d, 0) = U32(cst, 0x1C);
		U32(d, 4) = U32(cst, 0x20);
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)dz);
		U16(MEM<uint32_t>(0x15720A8), 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		return 0; // void
	}

	// 0x78DCE0 (MAG_084_sub_78DCE0): DIRECTOR task with 16 states - cut bookkeeping (0x78DDB0: a new cut spawns its parts,
	// 0x78EB50), then {0x78DE00 the texture + camera task, 0x78E850, 0x78E8A0 cut 1 (the other battle entities hidden),
	// 0x78E940 frame 0x36, 0x78E960, 0x78E980, 0x78E9A0 frame 0x19, 0x78E9C0, 0x78E9E0 cut 3, 0x78EA00 frame 0x41, 0x78EA20
	// cut 4, 0x78EA40 cut 4 (the entities shown again), 0x78EAC0 frame 0x3C, 0x78EAE0 cut 5 (done flag +0x48), 0x78EB10
	// finish, 0x78EB30 ret}
	uint32_t __cdecl te_78DCE0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[16];
		states[0] = 0x78DE00;
		states[1] = 0x78E850;
		states[2] = 0x78E8A0;
		states[3] = 0x78E940;
		states[4] = 0x78E960;
		states[5] = 0x78E980;
		states[6] = 0x78E9A0;
		states[7] = 0x78E9C0;
		states[8] = 0x78E9E0;
		states[9] = 0x78EA00;
		states[10] = 0x78EA20;
		states[11] = 0x78EA40;
		states[12] = 0x78EAC0;
		states[13] = 0x78EAE0;
		states[14] = 0x78EB10;
		states[15] = 0x78EB30;
		te_78DDB0(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl te_78DDB0(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			te_78EB50(a1);
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

	// 0x78DE00 (MAG_084_UploadAngelWingsTIM): director state 0 - queues the TIM 0x17243F8 (exe data) for upload, arms the
	// battle camera's return after the effect (0x50A730), spawns the camera task 0x78DE40 (0x64 bytes, queue 0x25C2598),
	// next state
	uint32_t __cdecl te_78DE00(uint32_t a1)
	{
		x::Battle_QueueTIMUpload_GetEOF(0x17243F8);
		gx::BS_Camera_ArmReturnAfterEffect();
		x::Effect_AddTaskAndInitFromCtx(0x25C2598, 0x78DE40, 0x64, a1);
		next_state(a1);
		return 0; // void
	}

	// 0x71DC40 (module 099 sub_71DC40): CAMERA task - runs its 10-state keyframe-camera script;
	// ends when finished with no children. No drawing.
	uint32_t __cdecl te_78DE40(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[10];
		states[0] = 0x78DED0;
		states[1] = 0x78DF00;
		states[2] = 0x78E700;
		states[3] = 0x78E730;
		states[4] = 0x78E760;
		states[5] = 0x78E790;
		states[6] = 0x78E7C0;
		states[7] = 0x78E7F0;
		states[8] = 0x78E810;
		states[9] = 0x78E840;  // nullsub
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x762E60 (module 090 sub_762E60): master state - a_73AB00, next state.
	uint32_t __cdecl te_78DED0(uint32_t a1)
	{
		a_73AB00();
		t1_next_state(a1);
		return 0; // void
	}

	// 0x78DF00 (MAG_084_sub_78DF00): camera state 1 - at cut 1 (a_73AAE0) starts the camera script 0x1575DE0 from the
	// director block, next state
	uint32_t __cdecl te_78DF00(uint32_t a1)
	{
		if (a_73AAE0(1) != 0)
		{
			a_73AB20(0x1575DE0, MEM<uint32_t>(DIR_PTR), 0);
			next_state(a1);
		}
		return 0; // void
	}

	// 0x774E90 (sub_774E90): camera state 8 - at the script's end starts 0x1560468 from the director block, next state
	uint32_t __cdecl te_78E700(uint32_t a1)
	{
		if (a_73AE10() == 1)
			ak_cam_chain(a1, 0x1575E28, MEM<uint32_t>(0x15720A8));
		return 0; // void
	}

	// 0x7332C0 (module 096 sub_7332C0): script state - steps the running script (a_73AE10); at
	// cue 2 starts script 0x1532CB0 on the director data and advances.
	uint32_t __cdecl te_78E730(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(2) != 0)
		{
			a_73AB20(0x1575E70, MEM<uint32_t>(0x15720A8), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x73B320 (module; Siren sub_73B320): camera state: steps the camera path (a_73AE10); once cut
	// >= 3 is requested, sets up camera path 0x1533CD0 against the script block itself (no +8),
	// advances the state
	uint32_t __cdecl te_78E760(uint32_t a1)
	{
		a_73AE10();
		uint32_t r = a_73AAE0(3);
		if (r == 0) return 0;
		uint32_t blk = MEM<uint32_t>(0x15720A8);
		a_73AB20(0x1575F20, blk, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x774E90 (sub_774E90): camera state 8 - at the script's end starts 0x1560468 from the director block, next state
	uint32_t __cdecl te_78E790(uint32_t a1)
	{
		if (a_73AE10() == 1)
			ak_cam_chain(a1, 0x1575F90, MEM<uint32_t>(0x15720A8));
		return 0; // void
	}

	// 0x78E7F0 (MAG_084_sub_78E7F0): camera state 7 - steps the camera script (a_73AE10); at cut 5 next state
	uint32_t __cdecl te_78E7F0(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(5) != 0)
			next_state(a1);
		return 0; // void
	}

	// 0x78E8A0 (MAG_084_sub_78E8A0): director state 2 - at cut 1 (a_73B7E0) hides every other battle entity (0x78E8F0,
	// all but the caster +0x2C), next state
	uint32_t __cdecl te_78E8A0(uint32_t a1)
	{
		if (a_73B7E0(1) != 0)
		{
			// (movzx ax: the pushed dword's high half is the stale eax; the callee reads the word)
			te_78E8F0((uint32_t)U8(a1, 0x2C));
			next_state(a1);
		}
		return 0; // void
	}

	// 0x78E8F0 (MAG_084_sub_78E8F0): every battle entity draw entry (0x1D972C0, 7 x 0x9C bytes) but a1 that is shown (flag
	// word bit 1) keeps its flag word in the director block +0x20 + 4 k and gets the hidden bit 2 (| 4)
	uint32_t __cdecl te_78E8F0(uint32_t a1)
	{
		const int32_t keep = (int16_t)(uint16_t)a1;
		uint32_t saved = MEM<uint32_t>(DIR_PTR) + 0x20;
		int32_t i = 0;
		for (uint32_t e = 0x1D972C0; e < 0x1D97704; e += 0x9C, i++, saved += 4)
		{
			if (i == keep)
				continue;
			const uint16_t w = U16(e, 0);
			if ((w & 2) == 0)
				continue;
			U32(saved, 0) = (uint32_t)w;
			U16(e, 0) = (uint16_t)(w | 4);
		}
		return 0;
	}

	// 0x82B400 (sub_82B400): director state - next state from phase tick 0x36
	uint32_t __cdecl te_78E940(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(0x15720A8), 0x46) >= 0x2D)
			next_state(a1);
		return 0; // void
	}

	// 0x78E9A0 (MAG_084_sub_78E9A0): director state 6 - next state from frame 0x19 of the cut
	uint32_t __cdecl te_78E9A0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) >= 0x19)
			next_state(a1);
		return 0; // void
	}

	// 0x763BD0 (module 090 sub_763BD0): director state - next state at director cue 3 (a_73B7E0).
	uint32_t __cdecl te_78E9E0(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x78EA00 (MAG_084_sub_78EA00): director state 9 - next state from frame 0x41 of the cut
	uint32_t __cdecl te_78EA00(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) >= 0x41)
			next_state(a1);
		return 0; // void
	}

	// 0x73BA50 (module; Siren sub_73BA50): director state: asks for cut 4, advances when accepted
	uint32_t __cdecl te_78EA20(uint32_t a1)
	{
		uint32_t r = a_73B640(4);
		if (r == 0) return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x78EA40 (MAG_084_sub_78EA40): director state 11 - at cut 4 (a_73B7E0) the hidden battle entities get their bit 2
	// back from the saved flag words (0x78EA70), next state
	uint32_t __cdecl te_78EA40(uint32_t a1)
	{
		if (a_73B7E0(4) != 0)
		{
			te_78EA70((uint32_t)U8(a1, 0x2C));
			next_state(a1);
		}
		return 0; // void
	}

	// 0x78EA70 (MAG_084_sub_78EA70): every battle entity draw entry but a1 that is shown (bit 1): bit 2 of its flag word
	// = bit 2 of the word 0x78E8F0 saved in the director block +0x20 + 4 k
	uint32_t __cdecl te_78EA70(uint32_t a1)
	{
		const int32_t keep = (int16_t)(uint16_t)a1;
		uint32_t saved = MEM<uint32_t>(DIR_PTR) + 0x20;
		int32_t i = 0;
		for (uint32_t e = 0x1D972C0; e < 0x1D97704; e += 0x9C, i++, saved += 4)
		{
			if (i == keep)
				continue;
			const uint16_t w = U16(e, 0);
			if ((w & 2) == 0)
				continue;
			U16(e, 0) = (uint16_t)(w ^ ((uint16_t)(U8(saved, 0) ^ (uint8_t)w) & 4));
		}
		return 0;
	}

	// 0x785230 (sub_785230): director state 11 - next state from frame 0x3C
	uint32_t __cdecl te_78EAC0(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(0x15720A8), 0x46) >= 0x3C)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x78EAE0 (MAG_084_sub_78EAE0): director state 13 - at cut 5 (a_73B640) sets the done word +0x48 of the director
	// block, next state
	uint32_t __cdecl te_78EAE0(uint32_t a1)
	{
		if (a_73B640(5) != 0)
		{
			U16(MEM<uint32_t>(DIR_PTR), 0x48) = 1;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x78EB50 (MAG_084_SpawnAngelWingsModel): the parts of a new cut (director +0x40): 1 - the model container 0x7961E0
	// (0x154 bytes, queue 0x25C9ED8, model 0x1572BB0 animation 3), the prim play 0x78ECD0 (layout 0x171F2B0) and the two
	// glow tasks 0x796FD0 / 0x7972B0 (0x58 bytes, queue 0x25C3938); 2 - the feather actor system 0x7913C0 (0x171DE1C);
	// 3 - the prim play 0x791260 (layout 0x17217A8) and a second feather actor system
	uint32_t __cdecl te_78EB50(uint32_t a1)
	{
		const int32_t cut = S16(MEM<uint32_t>(DIR_PTR), 0x40);
		if (cut == 1)
		{
			const uint32_t n = x::Effect_AddTaskAndInitFromCtx(0x25C9ED8, 0x7961E0, 0x154, a1);
			x::Effect_BindModelContainerSetAnim(n, 0x1572BB0, 3);
			te_78EC80(a1, 0x78ECD0, 0x171F2B0, 0x4A0, 0, 0);
			x::Effect_AddTaskAndInitFromCtx(0x25C3938, 0x796FD0, 0x58, a1);
			x::Effect_AddTaskAndInitFromCtx(0x25C3938, 0x7972B0, 0x58, a1);
		}
		else if (cut == 2)
			a_73C100(a1, 0x7913C0, 0x171DE1C, 8, 0xF, 0);
		else if (cut == 3)
		{
			te_78EC80(a1, 0x791260, 0x17217A8, 0x788, 2, 0);
			a_73C100(a1, 0x7913C0, 0x171DE1C, 8, 0xF, 0);
		}
		return 0; // void
	}

	// 0x78EC80 (MAG_084_sub_78EC80): spawns a prim-play task a2 (0x830 bytes, queue 0x25C8378) for parent a1: +0x74 layout
	// a3, +0x78 = (s16) a4, +0x80 = a5, +0x82 = a6
	uint32_t __cdecl te_78EC80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t n = x::Effect_AddTaskAndInitFromCtx(0x25C8378, a2, 0x830, a1);
		U16(n, 0x80) = (uint16_t)a5;
		U32(n, 0x74) = a3;
		S32(n, 0x78) = (int16_t)(uint16_t)a4;
		U16(n, 0x82) = (uint16_t)a6;
		return n;
	}

	// 0x78ED30 (MAG_084_sub_78ED30): play 0x78ECD0 state 0 - from frame 0x18: decodes the layout (+0x74, into +0x94, size
	// +0x78), places it at the caster (+0x2C: entity +0x1C..) 0x400 up and 0x200 on z, scale 0x1000, +0x88 = 0xFE00, plays
	// (0x78EDB0), next state
	uint32_t __cdecl te_78ED30(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x18)
			return 0; // void
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U32(a1, 0x1C) = U32(ent, 0x1C);
		U32(a1, 0x20) = U32(ent, 0x20);
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + 0xFC00);
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + 0x200);
		S32(a1, 0x58) = 0x1000;
		S32(a1, 0x54) = 0x1000;
		S32(a1, 0x50) = 0x1000;
		U16(a1, 0x88) = 0xFE00;
		te_78EDB0(a1);
		next_state(a1);
		return 0; // void
	}

	// 0x754A60 (sub_754A60): heal particle play step (MiniMog 0x734450 shape) - builds the effect matrix (rotations
	// +0x62 Y / +0x60 X / +0x64 Z, scale +0x50, position +0x1C/+0x1E/+0x20, camera 0x1D97778) and plays the node's
	// prim layout (+0x94) through the callback 0x754B90. Stack block (0x5C bytes) handed to the callback: +0x00 Mat4x3,
	// +0x38 = node+0x70, +0x44 = node+0x7C, +0x48 = [0x2594D74] (vertex lerp scratch), +0x4C..+0x58 words = node
	// +0x8C/+0x8E/+0x90/+0x92/+0x86/+0x80/+0x88; +0x20..+0x37 and +0x3C..+0x43 are never written (the callback does not
	// read them; zeroed here). Not paused (0)
	uint32_t __cdecl te_78EDB0(uint32_t a1)
	{
		// 30 fps layer: see mag084_angel_wing_held.inc
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
		U32(L, 0x48) = MEM<uint32_t>(0x25C9EFC);
		U32(L, 0x44) = U32(node, 0x7C);
		U16(L, 0x4C) = U16(node, 0x8C);
		U32(L, 0x38) = U32(node, 0x70);
		U16(L, 0x52) = U16(node, 0x92);
		U16(L, 0x4E) = U16(node, 0x8E);
		U16(L, 0x50) = U16(node, 0x90);
		U16(L, 0x56) = U16(node, 0x80);
		U16(L, 0x58) = U16(node, 0x88);
		return prim_play(node + 0x94, 0x78EEE0, L, 0);
	}

	// 0x78EEE0 (MAG_084_sub_78EEE0; Doom dm_8C5B10's shape): prim-model player draw callback (layout a1, record a2, block a3) -
	// picks the object's vertex frame (lerped by MAG_017_sub_701390), builds its matrix (rotation 0x701310, or 0x7015B0 with
	// record flag 0x40000; position rotated by the parent; scale: a diagonal matrix product 0x56C220 with flag 0x100, else
	// scale3DMatrix), fills a 0x68-byte Field_Alloc render header (flags 0x2000 / 0x2030 |0xC0 with alpha, colour, depth
	// offset block +0x58 (-0x7000 for model 1 of a kind-2 block), scale words 0x100) and draws it with 0x78F1B0 into OT
	// base+0x44 (shift 2) at the module packet cursor 0x25C2354
	uint32_t __cdecl te_78EEE0(uint32_t a1, uint32_t a2, uint32_t a3)
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

		if ((U32(rec, 4) & 0x40000) != 0)
			gx::MAG_063_sub_7015B0(rec + 0x10, M);
		else
			x::MAG_017_sub_701310(rec + 0x10, M);
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		U16(V, 0) = vx;
		U16(V, 2) = vy;
		U16(V, 4) = vz;
		x::GTE_SetRotMatrix(blk);
		x::GTE_LoadV0(V);
		x::GTE_MVMVA_RotV0();
		x::GTE_ReadMAC123(M + 0x14);
		x::GTE_MatrixMultiply(blk, M);
		const int32_t t2 = S32(M, 0x1C), t1 = S32(M, 0x18), t0 = S32(M, 0x14);
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
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		// depth offset; the second model (+2 = 1) of a block of kind +0x56 = 2 is pulled 0x7000 forward
		if (S16(blk, 0x56) == 2 && S16(rec, 2) == 1)
			S32(hdr, 0x10) = S16(blk, 0x58) - 0x7000;
		else
			S32(hdr, 0x10) = S16(blk, 0x58);
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x2A) = 0x100;
		U16(hdr, 0x28) = 0x100;
		MEM<uint32_t>(PACKET_CURSOR) = te_78F1B0(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 te_78F790, FT4 te_78FC10,
	// a_73D770, a_73D9C0, 0x73DCA0, te_790BC0) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl te_78F1B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = te_78F2E0(ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = te_78F500(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = te_78F790(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = te_78FC10(ctx, a2, a3, cur);
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
			cur = te_7906C0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return te_790BC0(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl te_78F2E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_78F500(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_78F790(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_78FC10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_7906C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_790BC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x766760 (module 090 sub_766760): state "wait": when a_73B7E0(3) is set, or else when
	// te_78EDB0(node) returns 0, marks the node finished (+0x26 |= 1) and advances the state (+0x29).
	uint32_t __cdecl te_791210(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_73B7E0(3) != 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
			return node;   // eax = node
		}
		const uint32_t r = te_78EDB0(node);
		if (r == 0)
		{
			const uint8_t st = (uint8_t)(U8(node, 0x29) + 1);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = st;
			return st;     // eax = 0 with al = new state
		}
		return r;
	}

	// 0x7912C0 (MAG_084_sub_7912C0): play 0x791260 state 0 - a_746A10, decodes the layout, places it at the caster, scale
	// 0x1000, +0x60 = 0xFF80, plays (0x78EDB0), next state
	uint32_t __cdecl te_7912C0(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
		a_746A10();
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U32(a1, 0x1C) = U32(ent, 0x1C);
		U32(a1, 0x20) = U32(ent, 0x20);
		S32(a1, 0x58) = 0x1000;
		S32(a1, 0x54) = 0x1000;
		S32(a1, 0x50) = 0x1000;
		U16(a1, 0x60) = 0xFF80;
		te_78EDB0(a1);
		next_state(a1);
		return 0; // void
	}

	// 0x791350 (MAG_084_sub_791350): play 0x791260 state 1 - at cut 4 (a_73B7E0): a_746AE0, finished (+0x26 |= 1), next
	// state; else plays (0x78EDB0)
	uint32_t __cdecl te_791350(uint32_t a1)
	{
		if (a_73B7E0(4) != 0)
		{
			a_746AE0();
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			next_state(a1);
			return 0; // void
		}
		te_78EDB0(a1);
		return 0; // void
	}

	// 0x8C8930 (copy of mb_825410 0x825410: copy of db_846A80 0x846A80: copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl te_791420(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			te_791F30(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl te_791600(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x25C9F3C);
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
	uint32_t __cdecl te_791880(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x25C9F3C);
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
	uint32_t __cdecl te_791F30(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x25C9F3C, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			te_796120();
			te_793780();
			// 30 fps layer: see mag084_angel_wing_held.inc
			FX_HELD(held_note_system(sys);)
			te_793640();
			te_791FC0();
			sys = U32(0x25C9F3C, 0);
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
		U16(0x25C9F00, 0) = (uint16_t)(U16(0x25C9F00, 0) + c14);
		U16(0x25C9EE8, 0) = (uint16_t)(U16(0x25C9EE8, 0) + c16);
		return done;
	}

	// 0x7D7240 (copy of wb_87A440: 0x87A440 (copy of Curaga cg_87ED10: 0x87ED10 (sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440))): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl te_791FC0(void)
	{
		uint32_t act = U32(U32(0x25C9F3C, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x25C9F3C, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					te_792020(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x7D72A0 (copy of wb_87A4A0: 0x87A4A0 (copy of Curaga cg_87ED70: 0x87ED70 (sub_87ED70; = Confuse c_860710))): the engine's 0x7354A0 (draw of a prim-model actor: header on the module
	// scratch stack, model +0x170, fade +0x1CE / colour +0x16C, one copy or one per sub-position
	// +0x19C..) drawing into the module arena
	uint32_t __cdecl te_792020(uint32_t a1, uint32_t a2)
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
	uint32_t __cdecl te_7922A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_7924B0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_792C10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_792E40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_793100(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_793360(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl te_793640(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x25C9F3C), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x25C9F3C);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						te_793700(act);   // (the original also pushes the bone entry, unused)
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
							te_793700(act);
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
	uint32_t __cdecl te_793700(uint32_t a1)
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
	uint32_t __cdecl te_793780(void)
	{
		uint32_t act = U32(U32(0x25C9F3C, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				te_794A50(act);
			else if (type == 1)
				te_7937C0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x8C9810 (copy of mb_8277D0 0x8277D0: copy of db_848EF0 0x848EF0: copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl te_7937C0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x25C9F3C);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (te_794900(act, bone) == 0)
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
			st = MEM<uint32_t>(0x25C9F3C);
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
	uint32_t __cdecl te_7947F0(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x25C9F3C, 0);                  // actor state block
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
	uint32_t __cdecl te_794900(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				te_794980(a1, a2);
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
			te_794980(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			te_794980(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x8CA9D0 (copy of mb_828D30 0x828D30: copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl te_794980(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return te_7949B0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x7949B0 (MAG_084_sub_7949B0; = Doom dm_8CAA00 with a pool of 3): the engine's 0x741A70 (allocates an emitter actor record of
	// 0x6C bytes from the pool [0x25C9EEC], cursor 0x25C3930) with the module's pool of 3 records
	uint32_t __cdecl te_7949B0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x25C9EEC);
		int32_t idx = MEM<int16_t>(0x25C3930);
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
			if (idx >= 3)
				idx = 0;
			tries++;
		} while (tries < 4);
		idx++;
		if (idx < 3)
			MEM<uint16_t>(0x25C3930) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x25C3930) = 0;
		return slot;
	}

	// 0x8CAAA0 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl te_794A50(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x795450, a1);
		else if (mode == 4)
			callp(0x7955A0, a1);
		te_794AD0(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x8CAB20 (copy of mb_828E80 0x828E80: copy of db_84A200 0x84A200: copy of cg_881820 0x881820: sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl te_794AD0(uint32_t a1)
	{
		uint32_t sys = U32(0x25C9F3C, 0);
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
				te_794B50(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			te_794B50(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x794B50 (MAG_084_sub_794B50; = Doom dm_8CABA0 = the engine's a_737FD0, each particle starting at a random bone of
	// Rinoa's model): allocates a particle actor record (0x795380) for the emitter a1 and initialises its a3 particles from
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl te_794B50(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(SCRATCH_SP, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(SCRATCH_SP, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = te_795380(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(ACTOR_STATE, 0);
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
				// 0x794C2F (Angel Wing): the emitter position +0x4C.. = a random point of Rinoa's model (director +0x18:
				// the model container, its model +0x30): one of the 14 bones of the table 0x1572B90 (rand() % 14)
				alignas(4) uint8_t pt[8] = {};
				const uint32_t mdl = U32(MEM<uint32_t>(DIR_PTR), 0x18) + 0x30;
				const int32_t r = (int32_t)x::CrtRand();
				x::GetEffectSpawnPosition(mdl, (uint32_t)(int32_t)MEM<int16_t>(0x1572B90 + (uint32_t)(r % 14) * 2), 0, P(pt));
				S32(a1, 0x4C) = shl32(S16(P(pt), 0), 16);
				S32(a1, 0x50) = shl32(S16(P(pt), 2), 16);
				S32(a1, 0x54) = shl32(S16(P(pt), 4), 16);
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
		te_796090(node, desc);
		U32(SCRATCH_SP, 0) = U32(SCRATCH_SP, 0) + 0x50;
		return 0; // void
	}

	// 0x8CB380 (copy of d_8BC590 0x8BC590: sub_8BC590): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x2742868], cursor 0x2742830) with the module's pool of 0x28 records
	uint32_t __cdecl te_795380(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x25C9F30);
		int32_t i = MEM<int16_t>(0x25C9EF8);
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
			if (i >= 0x27)
				i = 0;
			tries++;
			if (tries >= 0x28)
				break;
		}
		i++;
		if (i >= 0x27)
			MEM<uint16_t>(0x25C9EF8) = 0;
		else
			MEM<uint16_t>(0x25C9EF8) = (uint16_t)i;
		return slot;
	}

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl te_7955A0(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x25C9F3C, 0);            // ecx
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
				uint32_t g = U32(0x25C9F3C, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x25C9F3C, 0);
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
	uint32_t __cdecl te_796090(uint32_t a1, uint32_t a2)
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
			te_7949B0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			te_7949B0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			te_7949B0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			te_7949B0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x8CC120 (copy of mb_82A480 0x82A480: copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via te_7949B0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl te_796120(void)
	{
		uint32_t dir = MEM<uint32_t>(0x25C9F3C);  // eax (re-read only after the calls)
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
					te_7949B0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x25C9F3C);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				te_7949B0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x25C9F3C);
			}
		}
		return 0; // void
	}

	// 0x8CC1B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl te_7961B0(uint32_t a1)
	{
		if (te_791F30(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7961E0 (MAG_084_sub_7961E0): MODEL CONTAINER task (Rinoa's angel model 0x1572BB0) with 6 states {0x796BF0 set-up +
	// the caster's model swap, 0x796D00 the reveal grows, 0x796D80, 0x796DB0 cut 4, 0x796DD0 fade out, 0x796FC0 ret}; draws
	// unless hidden (0x796390); on frames 0x32 and 0x64 spawns the prim play 0x796290 (layout 0x1720570)
	uint32_t __cdecl te_7961E0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = 0x796BF0;
		states[1] = 0x796D00;
		states[2] = 0x796D80;
		states[3] = 0x796DB0;
		states[4] = 0x796DD0;
		states[5] = 0x796FC0;
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 4) == 0)
			te_796390(node);
		const uint16_t t = U16(node, 0x24);
		if (t == 0x32 || t == 0x64)
			te_78EC80(node, 0x796290, 0x1720570, 0x158, 1, 0);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7962F0 (MAG_084_sub_7962F0): play 0x796290 state 0 - decodes the layout, places it at the caster on the ground
	// (y 0), scale 0x1000, plays (0x78EDB0), next state
	uint32_t __cdecl te_7962F0(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U32(a1, 0x1C) = U32(ent, 0x1C);
		U32(a1, 0x20) = U32(ent, 0x20);
		U16(a1, 0x1E) = 0;
		S32(a1, 0x58) = 0x1000;
		S32(a1, 0x54) = 0x1000;
		S32(a1, 0x50) = 0x1000;
		te_78EDB0(a1);
		next_state(a1);
		return 0; // void
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl te_796360(uint32_t a1)
	{
		const uint32_t r = te_78EDB0(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x796390 (MAG_084_sub_796390): the model container's draw (unless hidden, +0x26 bit 2). REVEAL phase (+0x26 bit 3):
	// the director block +0x18 = the node, the model matrix +0x70 from the angles +0x3C (ComposeZYXRotationMatrix),
	// translation = position +0x4C.. (+0x84..), scaled by +0x114..; the reveal height 0x25C23B6 = the y of the model's point
	// 0xF1; the model through the reveal renderer's draw context 0x25C2360 (0x796620 / 0x796720). Else the creature draw
	// (a_746C10, draw block 0x25C9238) at the module packet cursor 0x25C2354
	uint32_t __cdecl te_796390(uint32_t a1)
	{
		const uint16_t fl = U16(a1, 0x26);
		if ((fl & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag084_angel_wing_held.inc
		FX_HELD(held_note_draw(HK_MODEL, a1);)
		if ((fl & 8) != 0)
		{
			alignas(4) uint8_t pt[8] = {};
			U32(MEM<uint32_t>(DIR_PTR), 0x18) = a1;
			const uint32_t mdl = a1 + 0x30;
			const uint32_t m = mdl + 0x40;
			x::ComposeZYXRotationMatrix(mdl + 0xC, m);
			S32(mdl, 0x54) = S16(mdl, 0x1C);
			S32(mdl, 0x58) = S16(mdl, 0x1E);
			S32(mdl, 0x5C) = S16(mdl, 0x20);
			x::scale3DMatrix(m, a1 + 0x114);
			x::GetEffectSpawnPosition(mdl, 0xF1, 0, P(pt));
			MEM<uint16_t>(0x25C23B6) = U16(P(pt), 2);
			te_796620(0x25C2360);
			return 0; // void
		}
		MEM<uint32_t>(0x25C2354) = a_746C10(a1, 0x25C9238, MEM<uint32_t>(0x25C2354));
		return 0; // void
	}

	// 0x8CC6B0 (copy of d_8B7290 0x8B7290: sub_8B7290): the dark reaper draw of render context a1 (0x273AE90): matrix = the
	// model block's (+0x48 -> +0x40) rotation through the camera, its translation through the camera
	// (MAC -> +0x34); the block's shadow (0x5088A0), both models (+0x64 and the second +0x78) through
	// 0x8B7390, then the bone matrices back from the pose
	uint32_t __cdecl te_796620(uint32_t a1)
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
		te_796720(U32(blk, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		const uint32_t second = U32(blk, 0x78);
		if (second != 0)
			te_796720(U32(second, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		x::BattleModel_BuildBoneMatricesFromPose(anim);
		return 0; // void
	}

	// 0x796720 (MAG_084_sub_796720): the REVEAL model renderer (model a1, OT a2; a3 unused; draw context a4: +0x20 camera *
	// model matrix, +0x44 -> {+4 vertex buffer}, +0x48 the model block (+0x28 colour, +0x7C object mask), +0x4C -> packet
	// cursor, +0x54 the reveal radius (0x25C23B4), +0x56 its height (0x25C23B6)). Header (0x74 bytes, Field_Alloc): +0x28
	// vertex scratch, +0x38..+0x44 vertex indices, +0x48 / +0x4C packet words, +0x50 object mask, +0x54.. distance terms,
	// +0x64 distance, +0x68 NCLIP, +0x6C OTZ, +0x70 / +0x72 radius / height. Per visible object: its vertices through their
	// bone matrices (GTE light matrix / background vector) into the vertex buffer (12 bytes: x, y, z, pad, grey level); a
	// vertex's grey level from its distance to the reveal centre (x, y - height): inside radius - 0x80 full (0x80), up to the
	// radius (radius - distance), beyond 0; level = 2 / 3 of it. Then its triangles (POLY_GT3, code 0x36) and quads
	// (POLY_GT4, 0x3E): front faces only, gouraud grey per vertex, OT z = OTZ >> 2
	uint32_t __cdecl te_796720(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		(void)a3;
		const uint32_t ctx = a4;
		const uint32_t blk = U32(ctx, 0x48);
		uint32_t pkt = MEM<uint32_t>(U32(ctx, 0x4C));            // [esp+0x18]
		const uint32_t vbuf = U32(U32(ctx, 0x44), 4);            // ebp
		uint32_t table = U32(a1, 4);                             // [esp+0x14]
		const uint32_t bones = U32(a1, 0) + 0x10;                // [esp+0x2C]
		const int32_t count = S32(table, 0);                     // [esp+0x28]
		const uint32_t hdr = x::Field_Alloc(0x74);
		table += 4;
		const uint32_t colour = U32(blk, 0x28) & 0xFFFFFF;
		U32(hdr, 0x50) = U32(blk, 0x7C);
		U32(hdr, 0x48) = colour | 0x24000000;
		U32(hdr, 0x4C) = colour | 0x2C000000;
		U16(hdr, 0x70) = U16(ctx, 0x54);
		U16(hdr, 0x72) = U16(ctx, 0x56);
		x::GTE_SetRotMatrix(ctx + 0x20);
		x::GTE_SetTransVector(ctx + 0x20);
		for (int32_t i = 0; i < count; i++)
		{
			const uint32_t off = U32(table, 0);
			table += 4;
			uint32_t obj = U32(a1, 4) + off;
			if (((U32(hdr, 0x50) >> (i & 31)) & 1) == 0)
				continue;
			uint32_t v = vbuf;                                   // [esp+0x10]
			int32_t groups = S16(obj, 0);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = S16(obj, 0);
				obj += 2;
				const uint32_t m = bones + (uint32_t)(bone * 0x30) + 0x10;
				gx::GTE_SetLightMatrix(m);
				gx::GTE_SetBackgroundVector(U32(m, 0x14), U32(m, 0x18), U32(m, 0x1C));
				int32_t nv = S16(obj, 0);
				obj += 2;
				for (; nv > 0; nv--)
				{
					U16(hdr, 0x28) = U16(obj, 0);
					U16(hdr, 0x2A) = U16(obj, 2);
					U16(hdr, 0x2C) = U16(obj, 4);
					obj += 6;
					x::GTE_LoadV0(hdr + 0x28);
					gx::GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(hdr + 0x28);
					const int32_t dx = S16(hdr, 0x28);
					const int32_t dy = sub32(S16(hdr, 0x2A), S16(hdr, 0x72));
					S32(hdr, 0x54) = dx;
					S32(hdr, 0x58) = dy;
					S32(hdr, 0x5C) = 0;
					const int32_t sq = add32(mul32(dx, dx), mul32(dy, dy));
					S32(hdr, 0x64) = sq;
					const int32_t dist = (int32_t)x::Sqrt((uint32_t)sq);
					S32(hdr, 0x64) = dist;
					const int32_t radius = S16(hdr, 0x70);
					if (dist <= radius - 0x80)
						S32(v, 8) = 0x80;
					else if (dist <= radius)
						S32(v, 8) = radius - dist;
					else
						S32(v, 8) = 0;
					S32(v, 8) = div3(shl32(S32(v, 8), 1));
					// the 4th word (+0x2E) is what the scratch held (the original copies it along)
					U32(v, 0) = U32(hdr, 0x28);
					U32(v, 4) = U32(hdr, 0x2C);
					v += 0xC;
				}
			}
			obj = (obj + 3) & ~3u;
			const int32_t n3 = S16(obj, 0);
			const int32_t n4 = S16(obj, 2);
			uint32_t rec = obj + 0xC;                            // ebx
			for (int32_t k = n3; k > 0; k--, rec += 0x10)
			{
				const uint32_t i2 = U16(rec, 4) & 0xFFF, i1 = U16(rec, 2) & 0xFFF, i0 = U16(rec, 0) & 0xFFF;
				U32(hdr, 0x40) = i2;
				U32(hdr, 0x3C) = i1;
				U32(hdr, 0x38) = i0;
				x::GTE_LoadV012(vbuf + i0 * 0xC, vbuf + i1 * 0xC, vbuf + i2 * 0xC);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(hdr + 0x68);
				if (S32(hdr, 0x68) <= 0)
					continue;
				U32(pkt, 0) = 0x9000000;
				U8(pkt, 7) = 0x36;
				x::GTE_AVSZ3();
				x::GTE_ReadOTZ(hdr + 0x6C);
				U32(hdr, 0x6C) = U32(hdr, 0x6C) >> 2;
				gx::GTE_StoreSXY012_PolyGT3(pkt);
				U32(pkt, 0x18) = U32(rec, 0xC);
				U32(pkt, 0xC) = U32(rec, 8);
				U16(pkt, 0x24) = U16(rec, 6);
				uint8_t g = U8(vbuf + U32(hdr, 0x38) * 0xC, 8);
				U8(pkt, 6) = g;
				U8(pkt, 5) = g;
				U8(pkt, 4) = g;
				g = U8(vbuf + U32(hdr, 0x3C) * 0xC, 8);
				U8(pkt, 0x12) = g;
				U8(pkt, 0x11) = g;
				U8(pkt, 0x10) = g;
				g = U8(vbuf + U32(hdr, 0x40) * 0xC, 8);
				U8(pkt, 0x1E) = g;
				U8(pkt, 0x1D) = g;
				U8(pkt, 0x1C) = g;
				x::SSIGPU_InsertPrimAutoDepth(a2 + U32(hdr, 0x6C) * 4, pkt);
				pkt += 0x28;
			}
			if (n4 > 0)
			{
				rec += 4;
				for (int32_t k = n4; k > 0; k--, rec += 0x14)
				{
					const uint32_t i2 = U16(rec, 0) & 0xFFF, i1 = U16(rec, -2) & 0xFFF, i0 = U16(rec, -4) & 0xFFF;
					U32(hdr, 0x40) = i2;
					U32(hdr, 0x3C) = i1;
					U32(hdr, 0x38) = i0;
					x::GTE_LoadV012(vbuf + i0 * 0xC, vbuf + i1 * 0xC, vbuf + i2 * 0xC);
					x::GTE_RTPT();
					x::GTE_NCLIP();
					x::GTE_ReadMAC0(hdr + 0x68);
					if (S32(hdr, 0x68) <= 0)
						continue;
					U8(pkt, 7) = 0x3E;
					U32(pkt, 0) = 0xC000000;
					x::GTE_StoreSXY012_PolyGT3_2(pkt);
					const uint32_t i3 = U16(rec, 2) & 0xFFF;
					U32(hdr, 0x44) = i3;
					x::GTE_LoadV0(vbuf + i3 * 0xC);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(pkt + 0x2C);
					x::GTE_AVSZ4();
					x::GTE_ReadOTZ(hdr + 0x6C);
					U32(pkt, 0x18) = U32(rec, 8);
					U32(pkt, 0xC) = U32(rec, 4);
					U16(pkt, 0x30) = U16(rec, 0xE);
					U16(pkt, 0x24) = U16(rec, 0xC);
					const uint32_t z = U32(hdr, 0x6C) >> 2;
					U32(hdr, 0x6C) = z;
					uint8_t g = U8(vbuf + U32(hdr, 0x38) * 0xC, 8);
					U8(pkt, 6) = g;
					U8(pkt, 5) = g;
					U8(pkt, 4) = g;
					g = U8(vbuf + U32(hdr, 0x3C) * 0xC, 8);
					U8(pkt, 0x12) = g;
					U8(pkt, 0x11) = g;
					U8(pkt, 0x10) = g;
					g = U8(vbuf + U32(hdr, 0x40) * 0xC, 8);
					U8(pkt, 0x1E) = g;
					U8(pkt, 0x1D) = g;
					U8(pkt, 0x1C) = g;
					g = U8(vbuf + U32(hdr, 0x44) * 0xC, 8);
					U8(pkt, 0x2A) = g;
					U8(pkt, 0x29) = g;
					U8(pkt, 0x28) = g;
					x::SSIGPU_InsertPrimAutoDepth(a2 + z * 4, pkt);
					pkt += 0x34;
				}
			}
		}
		MEM<uint32_t>(U32(ctx, 0x4C)) = pkt;
		return x::Field_Free(0x74);
	}

	// 0x796BF0 (MAG_084_sub_796BF0): model container state 0 - draw scale +0x114.. = 0x1000 (pointer +0x60), position
	// +0x4C.. = the caster's (+0x2C: entity +0x1C..), animation start (au_re_Battle_ReadAnimation_7), the reveal draw
	// context (0x796C70); the CASTER'S MODEL is swapped (BS_BindEntityModelFromComData of the entity with 0x25C9F10 /
	// 0x1576020) and its chain transformation queued (QueueChainTransformation 1), next state
	uint32_t __cdecl te_796BF0(uint32_t a1)
	{
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
		U32(a1, 0x60) = a1 + 0x114;
		U32(a1, 0x4C) = U32(ent, 0x1C);
		U32(a1, 0x50) = U32(ent, 0x20);
		S32(a1, 0x11C) = 0x1000;
		S32(a1, 0x118) = 0x1000;
		S32(a1, 0x114) = 0x1000;
		x::au_re_Battle_ReadAnimation_7(a1, 0);
		te_796C70(a1);
		gx::BS_BindEntityModelFromComData(ent, 0x25C9F10, 0x1576020);
		gx::QueueChainTransformation(ent, 1);
		next_state(a1);
		return 0; // void
	}

	// 0x70C810 (sub_70C810): the reaper's dark draw context (0x25570B0, vertex buffer 0x2557770,
	// object block 0x255EA98, model block = the reaper node +0x30); dark flag +0x26 bit 3
	uint32_t __cdecl te_796C70(uint32_t a1)
	{
		te_796CA0(REAPER_CTX, 0x25C3948, 0x25C9F40, a1 + 0x30);
		U8(a1, 0x26) |= 8;
		return 0; // void
	}

	// 0x79E590 (sub_79E590): draw block a1 (0x58 bytes) cleared; +0x48 = entity a4, +0x40 = vertex buffer a2,
	// +0x4C / +0x50 = the module packet cursor 0x25C9F74, +0x44 = descriptor a3 (+4 = a2, +0x14.. = 0, 0,
	// 0x140, 0, colour 0x80 x3, -1, 0)
	uint32_t __cdecl te_796CA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		x::MAG_007_sub_8DCC00(a1, 0x58);
		U32(a1, 0x48) = a4;
		U32(a1, 0x40) = a2;
		U32(a1, 0x4C) = PACKET_CURSOR;
		U32(a1, 0x50) = PACKET_CURSOR;
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

	// 0x796D00 (MAG_084_sub_796D00): model container state 1 - from the container's frame +0x136 = 0x15: the sound
	// 0x15720AC (BdPlaySE, every tick) and the reveal radius 0x25C23B4 grows by 0x20 up to 0x500; at cut 2 (a_73B7E0) the
	// reveal ends (+0x26 bit 3 cleared), next state; animation step
	uint32_t __cdecl te_796D00(uint32_t a1)
	{
		if (S16(a1, 0x136) >= 0x15)
		{
			x::BdPlaySE(0x15720AC, 0, 0x80);
			if (S16(a1, 0x136) >= 0x15)
			{
				const uint16_t r = (uint16_t)(MEM<uint16_t>(0x25C23B4) + 0x20);
				MEM<uint16_t>(0x25C23B4) = r;
				if ((int16_t)r >= 0x500)
					MEM<uint16_t>(0x25C23B4) = 0x500;
			}
		}
		if (a_73B7E0(2) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) & 0xF7);
			next_state(a1);
		}
		x::au_re_Battle_ReadAnimation_8(a1);
		return 0; // void
	}

	// 0x796DB0 (MAG_084_sub_796DB0): model container state 3 - at cut 4 (a_73B7E0) next state; animation step
	uint32_t __cdecl te_796DB0(uint32_t a1)
	{
		if (a_73B7E0(4) != 0)
			next_state(a1);
		x::au_re_Battle_ReadAnimation_8(a1);
		return 0; // void
	}

	// 0x79EC40 (sub_79EC40): creature state 4 - animation step, fade +0x13A up by 0x100 per tick; at 0x1000
	// hidden and finished (+0x26 |= 5), next state
	uint32_t __cdecl te_796DD0(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_8(a1); // (0x8DD220; the source text 0x79EC40 calls 0x8DD1C0)
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) + 0x100);
		if (S16(a1, 0x13A) >= 0x1000)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 5);
			U16(a1, 0x13A) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		U16(a1, 0x13C) = U16(a1, 0x13A);
		te_796E30(a1, 3);
		te_796F50(a1);
		return 0; // void
	}

	// 0x8CD310 (copy of t_7678B0 0x7678B0: module 090 sub_7678B0): applies the fade level +0x13A to the model container
	// (node+0x30) with mode a2 (a_733950).
	uint32_t __cdecl te_796E30(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		// the level argument's high half is ecx garbage; a_733950 only reads the low word
		a_733950(node + 0x30, U16(node, 0x13A), a2);
		return 0; // void
	}

	// 0x8CD430 (copy of t_768130 0x768130: module 090 sub_768130): model colour +0x5C..+0x5E = the battle ambient colour bytes
	// [0xB8B9A8..0xB8B9AA] each reduced by (byte * level +0x13C) / 4096.
	uint32_t __cdecl te_796F50(uint32_t a1)
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

	// 0x796FD0 (MAG_084_sub_796FD0): GLOW task A (7 states {0x797190 finish, 0x7971B0 from frame 0x20 the glow model
	// 0x15720B0 at full fade, 0x7971F0 fades out, 0x797220 cut 4, 0x797250 count, 0x797270 fades in + finish, 0x7972A0
	// ret}); every tick: its position +0x1C.. = the caster's point 0xF1 (GetEffectSpawnPosition), draw (0x797080)
	uint32_t __cdecl te_796FD0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[7];
		states[0] = 0x797190;
		states[1] = 0x7971B0;
		states[2] = 0x7971F0;
		states[3] = 0x797220;
		states[4] = 0x797250;
		states[5] = 0x797270;
		states[6] = 0x7972A0;
		callp(states[S8(node, 0x29)], node);
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C;
		x::GetEffectSpawnPosition(ent, 0xF1, 0, node + 0x1C);
		te_797080(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x797080 (MAG_084_sub_797080): glow A draw (unless hidden, +0x26 bit 2): a camera-facing matrix (the camera's
	// rotation unpacked, turned by +0x48 about z, translation = position +0x1C.., scaled by +0x30..) through the camera
	// (ComposeAffineTransform); header 0x68 (model +0x4C, +8 colour +0x40, +0xC fade +0x50, +0x10 depth +0x52, flags 0xF0,
	// windows 256 x 256) through the module renderer 0x78F1B0 into OT base +0x44 (shift 2) at the module packet cursor
	// 0x25C2354
	uint32_t __cdecl te_797080(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag084_angel_wing_held.inc
		FX_HELD(held_note_draw(HK_GLOW, a1);)
		alignas(4) uint8_t loc[0x20] = {};
		const uint32_t M = P(loc);
		x::UnpackRotationMatrix(0x1D97778, M);
		x::sub_8DD960(M, (uint32_t)(int32_t)S16(a1, 0x48));
		S32(M, 0x14) = S16(a1, 0x1C);
		S32(M, 0x18) = S16(a1, 0x1E);
		x::scale3DMatrix(M, a1 + 0x30);
		S32(M, 0x1C) = S16(a1, 0x20);
		x::ComposeAffineTransform(0x1D97778, M, M);
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const uint32_t hdr = x::Field_Alloc(0x68);
		U32(hdr, 0) = U32(a1, 0x4C);
		U32(hdr, 8) = U32(a1, 0x40);
		S32(hdr, 0xC) = S16(a1, 0x50);
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x2A) = 0x100;
		U16(hdr, 0x28) = 0x100;
		S32(hdr, 0x10) = S16(a1, 0x52);
		U16(hdr, 0x18) = 0;
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1E) = 0;
		U16(hdr, 0x1C) = 0;
		U16(hdr, 0x26) = 0;
		U16(hdr, 0x24) = 0;
		U32(hdr, 0x14) = 0xF0;
		MEM<uint32_t>(PACKET_CURSOR) = te_78F1B0(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x85E620 (copy of a_7475A0 0x7475A0: engine; Siren MAG_095_sub_7475A0): state handler: marks the node finished
	// (+0x26 bit0) and goes to the next state.
	uint32_t __cdecl te_797190(uint32_t a1)
	{
		const uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 4;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x7971B0 (MAG_084_sub_7971B0): glow A state 1 - from frame 0x20: shown (+0x26 bit 2 cleared), scale 0x1000, fade
	// +0x50 = 0x1000, model 0x15720B0, depth +0x52 = 0xFE00, next state
	uint32_t __cdecl te_7971B0(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x20)
			return 0; // void
		U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) & 0xFB);
		S32(a1, 0x38) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		U16(a1, 0x50) = 0x1000;
		U32(a1, 0x4C) = 0x15720B0;
		U16(a1, 0x52) = 0xFE00;
		next_state(a1);
		return 0; // void
	}

	// 0x7971F0 (MAG_084_sub_7971F0): glow state - fade +0x50 down by 0x80; at 0 next state
	uint32_t __cdecl te_7971F0(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) - 0x80);
		if (S16(a1, 0x50) <= 0)
		{
			U16(a1, 0x50) = 0;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x797220 (MAG_084_sub_797220): glow A state 3 - at cut 4 (a_73B7E0) +0x54 = 0x18 (the count of 0x797250), next state
	uint32_t __cdecl te_797220(uint32_t a1)
	{
		if (a_73B7E0(4) != 0)
		{
			U16(a1, 0x54) = 0x18;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x8CE1E0 (copy of t_7680B0 0x7680B0: module 090 sub_7680B0): creature state 11 - counts +0x54 down, next state at 0.
	uint32_t __cdecl te_797250(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x54) = (uint16_t)(U16(node, 0x54) - 1);
		if (S16(node, 0x54) <= 0)
			t4_next_state(node);
		return 0; // void
	}

	// 0x745610 (module 095): +0x50 += 0x200; at >= 0x1000: finished (+0x26 |= 1), clamp 0x1000,
	// next state
	uint32_t __cdecl te_797270(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
		if (S16(a1, 0x50) >= 0x1000)
		{
			int8_t st = S8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U16(a1, 0x50) = 0x1000;
			S8(a1, 0x29) = (int8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x7972B0 (MAG_084_sub_7972B0): GLOW task B (7 states {0x797410 finish, 0x797430 from frame 0x20 the glow model
	// 0x1572810 at the camera block's height, 0x797490 fades out, 0x7974C0 cut 3 hidden, 0x7974E0 cut 4 shown, 0x797500
	// fades in + finish, 0x797530 ret}); draws (0x797330) every tick
	uint32_t __cdecl te_7972B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[7];
		states[0] = 0x797410;
		states[1] = 0x797430;
		states[2] = 0x797490;
		states[3] = 0x7974C0;
		states[4] = 0x7974E0;
		states[5] = 0x797500;
		states[6] = 0x797530;
		callp(states[S8(node, 0x29)], node);
		te_797330(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7716C0 (sub_7716C0): moon model draw (unless hidden): identity rotation at +0x1C.., scaled by +0x30..; header 0x68
	// (model +0x4C, +8 colour +0x40, +0xC fade +0x50, +0x10 depth +0x52, flags 0xF0, windows 256 x 256) through the module
	// renderer 0x76A650 into OT base +0x44 (shift 2) at the module packet cursor 0x25A4D9C
	uint32_t __cdecl te_797330(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag084_angel_wing_held.inc
		FX_HELD(held_note_draw(HK_MOON, a1);)
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
		MEM<uint32_t>(PACKET_CURSOR) = te_78F1B0(hdr, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x85E620 (copy of a_7475A0 0x7475A0: engine; Siren MAG_095_sub_7475A0): state handler: marks the node finished
	// (+0x26 bit0) and goes to the next state.
	uint32_t __cdecl te_797410(uint32_t a1)
	{
		const uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 4;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x797430 (MAG_084_sub_797430): glow B state 1 - from frame 0x20: shown, position (0, camera block +0xE0, 0), scale
	// 0x1000, fade 0x1000, model 0x1572810, depth 0xFE00, next state
	uint32_t __cdecl te_797430(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x20)
			return 0; // void
		U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) & 0xFB);
		U16(a1, 0x1C) = 0;
		U16(a1, 0x1E) = 0;
		const uint32_t cam = MEM<uint32_t>(0x1572BAC);
		S32(a1, 0x38) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x30) = 0x1000;
		const uint16_t h = U16(cam, 0xE0);
		U16(a1, 0x50) = 0x1000;
		next_state(a1);
		U32(a1, 0x4C) = 0x1572810;
		U16(a1, 0x20) = h;
		U16(a1, 0x52) = 0xFE00;
		return 0; // void
	}

	// 0x797490 (MAG_084_sub_797490): glow B state 2 - fade +0x50 down by 0x80; at 0 next state
	uint32_t __cdecl te_797490(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) - 0x80);
		if (S16(a1, 0x50) <= 0)
		{
			U16(a1, 0x50) = 0;
			next_state(a1);
		}
		return 0; // void
	}

	// 0x7974C0 (MAG_084_sub_7974C0): glow B state 3 - at cut 3 (a_73B7E0) hidden (+0x26 |= 4), next state
	uint32_t __cdecl te_7974C0(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 4);
			next_state(a1);
		}
		return 0; // void
	}

	// 0x7974E0 (MAG_084_sub_7974E0): glow B state 4 - at cut 4 (a_73B7E0) shown again, next state
	uint32_t __cdecl te_7974E0(uint32_t a1)
	{
		if (a_73B7E0(4) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) & 0xFB);
			next_state(a1);
		}
		return 0; // void
	}

	// 0x797500 (MAG_084_sub_797500): glow B state 5 - fade +0x50 up by 0x80; at 0x1000 finished (+0x26 |= 1), next state
	uint32_t __cdecl te_797500(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x80);
		if (S16(a1, 0x50) >= 0x1000)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			next_state(a1);
			U16(a1, 0x50) = 0x1000;
		}
		return 0; // void
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl te_797540(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x15720A8);
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

	// ---- generated (desc84.py / cls84.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x78DAD0, (void *)a_73A0D0, "084 MAG_084_sub_78DAD0" },
		{ 0x78DAE0, (void *)a_73A0D0, "084 MAG_084_sub_78DAE0" },
		{ 0x78DB70, (void *)a_73A170, "084 MAG_084_sub_78DB70" },
		{ 0x78DBA0, (void *)a_73A1A0, "084 MAG_084_sub_78DBA0" },
		{ 0x78DEE0, (void *)a_73AB00, "084 sub_78DEE0" },
		{ 0x78DF30, (void *)a_73AAE0, "084 sub_78DF30" },
		{ 0x78DF50, (void *)a_73AB20, "084 sub_78DF50" },
		{ 0x78E240, (void *)a_73AE10, "084 sub_78E240" },
		{ 0x78E590, (void *)a_73B160, "084 sub_78E590" },
		{ 0x78E7C0, (void *)a_73B350, "084 sub_78E7C0" },
		{ 0x78E810, (void *)a_73B750, "084 sub_78E810" },
		{ 0x78E830, (void *)a_73A6D0, "084 nullsub_1330" },
		{ 0x78E840, (void *)a_73A6D0, "084 nullsub_1331" },
		{ 0x78E850, (void *)a_73B790, "084 sub_78E850" },
		{ 0x78E870, (void *)a_73B640, "084 sub_78E870" },
		{ 0x78E8D0, (void *)a_73B7E0, "084 sub_78E8D0" },
		{ 0x78E960, (void *)a_73B8A0, "084 sub_78E960" },
		{ 0x78E980, (void *)a_73B8C0, "084 sub_78E980" },
		{ 0x78E9C0, (void *)a_733B70, "084 sub_78E9C0" },
		{ 0x78EB10, (void *)a_7475A0, "084 sub_78EB10" },
		{ 0x78EB30, (void *)a_73A6D0, "084 nullsub_1332" },
		{ 0x78EB40, (void *)a_73A6D0, "084 nullsub_1333" },
		{ 0x78EC30, (void *)a_73C100, "084 sub_78EC30" },
		{ 0x78ECD0, (void *)a_73A380, "084 sub_78ECD0" },
		{ 0x790190, (void *)a_73D770, "084 sub_790190" },
		{ 0x7903E0, (void *)a_73D9C0, "084 sub_7903E0" },
		{ 0x791250, (void *)a_73A6D0, "084 nullsub_1334" },
		{ 0x791260, (void *)a_73A380, "084 sub_791260" },
		{ 0x791330, (void *)a_746A10, "084 sub_791330" },
		{ 0x791380, (void *)a_746AE0, "084 sub_791380" },
		{ 0x7913B0, (void *)a_73A6D0, "084 nullsub_1335" },
		{ 0x7913C0, (void *)a_73A380, "084 sub_7913C0" },
		{ 0x791450, (void *)a_73F990, "084 sub_791450" },
		{ 0x7916E0, (void *)a_73FC20, "084 sub_7916E0" },
		{ 0x791950, (void *)a_73FE90, "084 sub_791950" },
		{ 0x791A70, (void *)a_73FFB0, "084 sub_791A70" },
		{ 0x791CD0, (void *)a_740210, "084 sub_791CD0" },
		{ 0x792160, (void *)a_7355E0, "084 sub_792160" },
		{ 0x792730, (void *)a_735BB0, "084 sub_792730" },
		{ 0x792970, (void *)a_735DF0, "084 sub_792970" },
		{ 0x793850, (void *)a_740910, "084 sub_793850" },
		{ 0x793EB0, (void *)a_740F70, "084 sub_793EB0" },
		{ 0x793F00, (void *)a_740FC0, "084 sub_793F00" },
		{ 0x793F90, (void *)a_741050, "084 sub_793F90" },
		{ 0x794060, (void *)a_741120, "084 sub_794060" },
		{ 0x794AA0, (void *)a_741B60, "084 sub_794AA0" },
		{ 0x795220, (void *)a_742290, "084 au_re__rand_21" },
		{ 0x795250, (void *)a_7422C0, "084 sub_795250" },
		{ 0x795290, (void *)a_742300, "084 sub_795290" },
		{ 0x7952E0, (void *)a_742350, "084 sub_7952E0" },
		{ 0x795330, (void *)a_7423A0, "084 au_re__rand_21_0" },
		{ 0x795440, (void *)a_7424B0, "084 sub_795440" },
		{ 0x795450, (void *)a_7424C0, "084 sub_795450" },
		{ 0x795C30, (void *)a_742CA0, "084 sub_795C30" },
		{ 0x795C60, (void *)a_742CD0, "084 sub_795C60" },
		{ 0x795C90, (void *)a_742D00, "084 sub_795C90" },
		{ 0x795E40, (void *)a_742EB0, "084 sub_795E40" },
		{ 0x795F80, (void *)a_742FF0, "084 sub_795F80" },
		{ 0x7961D0, (void *)a_73A6D0, "084 nullsub_1336" },
		{ 0x796290, (void *)a_73A380, "084 sub_796290" },
		{ 0x796380, (void *)a_73A6D0, "084 nullsub_1337" },
		{ 0x796440, (void *)a_746C10, "084 sub_796440" },
		{ 0x796D80, (void *)a_739B40, "084 sub_796D80" },
		{ 0x796E50, (void *)a_733950, "084 sub_796E50" },
		{ 0x796FC0, (void *)a_73A6D0, "084 nullsub_1338" },
		{ 0x7972A0, (void *)a_73A6D0, "084 nullsub_1339" },
		{ 0x797530, (void *)a_73A6D0, "084 nullsub_1340" },
		{ 0x797580, (void *)a_7474B0, "084 MAG_084_sub_797580" },
		{ 0x7975A0, (void *)a_7474D0, "084 MAG_084_sub_7975A0" },
		{ 0x7975C0, (void *)a_73A6D0, "084 nullsub_1341" },
		{ 0x7975D0, (void *)a_747500, "084 MAG_084_sub_7975D0" },
		{ 0x797610, (void *)a_73A0D0, "084 MAG_084_sub_797610" },
		{ 0x797620, (void *)a_747550, "084 MAG_084_sub_797620" },
		{ 0x797630, (void *)a_747560, "084 MAG_084_sub_797630" },
		{ 0x797660, (void *)a_747590, "084 MAG_084_sub_797660" },
		{ 0x797670, (void *)a_7475A0, "084 MAG_084_sub_797670" },
		{ 0x797690, (void *)a_73A6D0, "084 nullsub_1329" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x78D940, (void *)te_78D940, "084 te_78D940" },
		{ 0x78DAF0, (void *)te_78DAF0, "084 te_78DAF0" },
		{ 0x78DB30, (void *)te_78DB30, "084 te_78DB30" },
		{ 0x78DC20, (void *)te_78DC20, "084 te_78DC20" },
		{ 0x78DC50, (void *)te_78DC50, "084 te_78DC50" },
		{ 0x78DCE0, (void *)te_78DCE0, "084 te_78DCE0" },
		{ 0x78DDB0, (void *)te_78DDB0, "084 te_78DDB0" },
		{ 0x78DE00, (void *)te_78DE00, "084 te_78DE00" },
		{ 0x78DE40, (void *)te_78DE40, "084 te_78DE40" },
		{ 0x78DED0, (void *)te_78DED0, "084 te_78DED0" },
		{ 0x78DF00, (void *)te_78DF00, "084 te_78DF00" },
		{ 0x78E700, (void *)te_78E700, "084 te_78E700" },
		{ 0x78E730, (void *)te_78E730, "084 te_78E730" },
		{ 0x78E760, (void *)te_78E760, "084 te_78E760" },
		{ 0x78E790, (void *)te_78E790, "084 te_78E790" },
		{ 0x78E7F0, (void *)te_78E7F0, "084 te_78E7F0" },
		{ 0x78E8A0, (void *)te_78E8A0, "084 te_78E8A0" },
		{ 0x78E8F0, (void *)te_78E8F0, "084 te_78E8F0" },
		{ 0x78E940, (void *)te_78E940, "084 te_78E940" },
		{ 0x78E9A0, (void *)te_78E9A0, "084 te_78E9A0" },
		{ 0x78E9E0, (void *)te_78E9E0, "084 te_78E9E0" },
		{ 0x78EA00, (void *)te_78EA00, "084 te_78EA00" },
		{ 0x78EA20, (void *)te_78EA20, "084 te_78EA20" },
		{ 0x78EA40, (void *)te_78EA40, "084 te_78EA40" },
		{ 0x78EA70, (void *)te_78EA70, "084 te_78EA70" },
		{ 0x78EAC0, (void *)te_78EAC0, "084 te_78EAC0" },
		{ 0x78EAE0, (void *)te_78EAE0, "084 te_78EAE0" },
		{ 0x78EB50, (void *)te_78EB50, "084 te_78EB50" },
		{ 0x78EC80, (void *)te_78EC80, "084 te_78EC80" },
		{ 0x78ED30, (void *)te_78ED30, "084 te_78ED30" },
		{ 0x78EDB0, (void *)te_78EDB0, "084 te_78EDB0" },
		{ 0x78EEE0, (void *)te_78EEE0, "084 te_78EEE0" },
		{ 0x78F1B0, (void *)te_78F1B0, "084 te_78F1B0" },
		{ 0x78F2E0, (void *)te_78F2E0, "084 te_78F2E0" },
		{ 0x78F500, (void *)te_78F500, "084 te_78F500" },
		{ 0x78F790, (void *)te_78F790, "084 te_78F790" },
		{ 0x78FC10, (void *)te_78FC10, "084 te_78FC10" },
		{ 0x7906C0, (void *)te_7906C0, "084 te_7906C0" },
		{ 0x790BC0, (void *)te_790BC0, "084 te_790BC0" },
		{ 0x791210, (void *)te_791210, "084 te_791210" },
		{ 0x7912C0, (void *)te_7912C0, "084 te_7912C0" },
		{ 0x791350, (void *)te_791350, "084 te_791350" },
		{ 0x791420, (void *)te_791420, "084 te_791420" },
		{ 0x791600, (void *)te_791600, "084 te_791600" },
		{ 0x791880, (void *)te_791880, "084 te_791880" },
		{ 0x791F30, (void *)te_791F30, "084 te_791F30" },
		{ 0x791FC0, (void *)te_791FC0, "084 te_791FC0" },
		{ 0x792020, (void *)te_792020, "084 te_792020" },
		{ 0x7922A0, (void *)te_7922A0, "084 te_7922A0" },
		{ 0x7924B0, (void *)te_7924B0, "084 te_7924B0" },
		{ 0x792C10, (void *)te_792C10, "084 te_792C10" },
		{ 0x792E40, (void *)te_792E40, "084 te_792E40" },
		{ 0x793100, (void *)te_793100, "084 te_793100" },
		{ 0x793360, (void *)te_793360, "084 te_793360" },
		{ 0x793640, (void *)te_793640, "084 te_793640" },
		{ 0x793700, (void *)te_793700, "084 te_793700" },
		{ 0x793780, (void *)te_793780, "084 te_793780" },
		{ 0x7937C0, (void *)te_7937C0, "084 te_7937C0" },
		{ 0x7947F0, (void *)te_7947F0, "084 te_7947F0" },
		{ 0x794900, (void *)te_794900, "084 te_794900" },
		{ 0x794980, (void *)te_794980, "084 te_794980" },
		{ 0x7949B0, (void *)te_7949B0, "084 te_7949B0" },
		{ 0x794A50, (void *)te_794A50, "084 te_794A50" },
		{ 0x794AD0, (void *)te_794AD0, "084 te_794AD0" },
		{ 0x794B50, (void *)te_794B50, "084 te_794B50" },
		{ 0x795380, (void *)te_795380, "084 te_795380" },
		{ 0x7955A0, (void *)te_7955A0, "084 te_7955A0" },
		{ 0x796090, (void *)te_796090, "084 te_796090" },
		{ 0x796120, (void *)te_796120, "084 te_796120" },
		{ 0x7961B0, (void *)te_7961B0, "084 te_7961B0" },
		{ 0x7961E0, (void *)te_7961E0, "084 te_7961E0" },
		{ 0x7962F0, (void *)te_7962F0, "084 te_7962F0" },
		{ 0x796360, (void *)te_796360, "084 te_796360" },
		{ 0x796390, (void *)te_796390, "084 te_796390" },
		{ 0x796620, (void *)te_796620, "084 te_796620" },
		{ 0x796720, (void *)te_796720, "084 te_796720" },
		{ 0x796BF0, (void *)te_796BF0, "084 te_796BF0" },
		{ 0x796C70, (void *)te_796C70, "084 te_796C70" },
		{ 0x796CA0, (void *)te_796CA0, "084 te_796CA0" },
		{ 0x796D00, (void *)te_796D00, "084 te_796D00" },
		{ 0x796DB0, (void *)te_796DB0, "084 te_796DB0" },
		{ 0x796DD0, (void *)te_796DD0, "084 te_796DD0" },
		{ 0x796E30, (void *)te_796E30, "084 te_796E30" },
		{ 0x796F50, (void *)te_796F50, "084 te_796F50" },
		{ 0x796FD0, (void *)te_796FD0, "084 te_796FD0" },
		{ 0x797080, (void *)te_797080, "084 te_797080" },
		{ 0x797190, (void *)te_797190, "084 te_797190" },
		{ 0x7971B0, (void *)te_7971B0, "084 te_7971B0" },
		{ 0x7971F0, (void *)te_7971F0, "084 te_7971F0" },
		{ 0x797220, (void *)te_797220, "084 te_797220" },
		{ 0x797250, (void *)te_797250, "084 te_797250" },
		{ 0x797270, (void *)te_797270, "084 te_797270" },
		{ 0x7972B0, (void *)te_7972B0, "084 te_7972B0" },
		{ 0x797330, (void *)te_797330, "084 te_797330" },
		{ 0x797410, (void *)te_797410, "084 te_797410" },
		{ 0x797430, (void *)te_797430, "084 te_797430" },
		{ 0x797490, (void *)te_797490, "084 te_797490" },
		{ 0x7974C0, (void *)te_7974C0, "084 te_7974C0" },
		{ 0x7974E0, (void *)te_7974E0, "084 te_7974E0" },
		{ 0x797500, (void *)te_797500, "084 te_797500" },
		{ 0x797540, (void *)te_797540, "084 te_797540" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag084_angel_wing()
	{
		act::register_module(84);
		for (const act::angelwing::ModPort *p = act::angelwing::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(84, p->addr, p->port, p->name);
		for (const act::angelwing::ModPort *p = act::angelwing::PORTS; p->addr; p++)
			act::register_module_port(84, p->addr, p->port, p->name);
		// 30 fps layer: see mag084_angel_wing_held.inc
		FX_HELD(register_mag084_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag084_angel_wing_held.inc"
#endif
