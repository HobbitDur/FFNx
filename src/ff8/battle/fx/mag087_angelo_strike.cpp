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

// Effect 87: Angelo Strike (kernel.bin Rinoa limit break part 2 {Combine} Angelo Strike, section 25 entry 1, effect id +2 = 87;
// MAG_087_ANGELO_STRIKE*): a copy of the actor/heal effect library (see act_engine.h) with its own parts: Angelo runs at the
// target, digs in under it (a ground capture of the stage), lifts it, swings it round over his head and slams it down
// (debris, dust, an impact play), then jumps back to Rinoa and walks off. Module 0x773850..0x77F230 (206 functions in IDA, no
// missing / bogus start: 93 served by engine ports, 51 the relocated text of a ported library function (Doom 8, Siren 95,
// Ultimecia 80 / The End 85's library, Angelo Recover 93 / Reverse 94 / Rush 91 / Search 92, Invincible Moon 88, Boko 97..100,
// Laser Eye 71, Tonberry 90, Lv Up 41), 60 hand-ported (the master is the module's own port of the engine master a_739F40), the
// loader and the setup not ported).
//
// Setup MAG_087_ANGELO_STRIKE 0x773870 (runs once, not ported; loader 0x773850) creates the root queue with the master and the
// module's task pools (no engine camera animation: the module's own camera task drives the camera).
//   Master (0x773A10 = the engine master a_739F40) - camera copy 0x2793E58 (pointers 0x25B4B24 / 0x25ACFC0), packet arenas by
//     tick parity (module cursor 0x25ACEB4 / 0x25ACFBC), target bone follow, 11 states (0x773CF0: the director block 0x774190,
//     the module data block 0x773D40, the load script task 0x773E30 (a_73A380 / a_73A3E0, script 0x155B190) and the DIRECTOR
//     0x774240), the nine module queues.
//   Director (0x774240, 10 states) - Angelo's TIM 0x16FDF78, the camera return armed (0x50A730), the CAMERA task 0x7743A0 (camera
//     script chain 0x15600F8.. through the library camera stepper a_73AE10 / a_73AB20 on the camera block [0x155B1E0], from the
//     director block or Angelo's point +0x148), sync points 1..3, cues 1..3, the load script step; from frame 0x1C it waits for
//     the loads and meanwhile PAUSES the module (0x775050 = a_73BB60: switch word 0x25B4B4C stops the camera states 2 / 3 and
//     Angelo's state 2, the plays' pause word 0x25AECD4); done flag +0x48 at frame 0x48.
//   Phase 1 (0x775160) - ANGELO 0x7751A0 (node 0x178, model container 0x155B1E4), drawn by the creature renderer 0x775330 =
//     a_746C10 (state block 0x25B3958) into the module packet pool. He binds Rinoa's animation set (com data 0x1560550) and
//     queues her chain transformations 1 and 2; ring particles 0x775720 (prim sequence drawn by 0x775780) on his foot-down
//     frames (0x775510 / 0x775F40); the ground capture 0x7789B0 / 0x778AB0 (the stage model 0x1D989C0 rendered from above
//     into the 64 x 64 texture at (0x300, 0x180), frame arena 0x1D8E054); three prim plays (0x776460: node 0x25C, callback
//     0x7766B0): two dust plays 0x7764B0 (layout 0x16FD248) at his point 0xF1 and the impact play 0x77E6C0 (layout [loader block
//     +0x128]) scaled by the target's size; four actor systems 0x779050 (a_73C100: data 0x16FBE40) at his foot points; the halo
//     0x77DFC0 and the debris 0x77EAB0 / 0x77EC00 (spawner 0x77E800; models drawn by 0x77E060 into the frame arena).
//   Damage state 0x77F0D0.. (the library's finish states): ApplyActionResultToTarget at the director's done flag.
// Writes outside the module: Rinoa's entity record (BS_BindEntityModelFromComData 0x5022C0 binds the animation set,
// QueueChainTransformation 0x505C00 chain transformations 1 and 2); the TARGET's entity record (the lowest entity of the
// target's chain, Battle_EntityChainLowest 0x501FF0): facing +0xE (state 0), carried by Angelo (0x77E260: pitch +0xC,
// position +0x1C.. / +0x54.., matrix +0x40.. (ComposeZYXRotationMatrix), the same for the entities chained to it with flag
// bit 1) and put back (state 13); the camera words 0xB8B7F0.. / 0x1D8E038 / 0x1D977A2 (camera script) and the armed camera
// return; the GTE screen offset (0x56CCE0, set and put back by the ground capture); the root-matrix slot 0x1D99AB0 (Angelo);
// the camera copy 0x2793E58; action results (0x506690).
// Module globals: 0x25ACEB0..0x25B4CC4 (+ camera copy 0x2793E58, 0x20 bytes).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace strike87
{
	// ---- generated (desc87.py): the module descriptor of this copy of the library ----
	static const Mod MOD_087 = { "angelostrike", 87, 0x773850, 0x77F230,
		{ 0x0, 0x0, 0x1557D28, 0x155B1E0, 0x0, 0x25ACFBC, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25ACEB0, 0x25ACEB4, 0x25ACF98, 0x25ACFBC, 0x25ACFC0, 0x25AECD8, 0x0, 0x25AECD4, 0x25B2A90, 0x25AD0A0, 0x25B2AA0, 0x25B2AA4, 0x25B3538, 0x25B37C8, 0x25B37D8, 0x25B3948, 0x25B394C, 0x25B3950, 0x25B4448, 0x25B4620, 0x25B4630, 0x0, 0x25B46A8, 0x25B4B08, 0x25B4B10, 0x25B4B24, 0x25B4B4C, 0x25B4B54 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x779D00, 0x779E40, 0x779F80, 0x77A190, 0x77A410, 0x77A650, 0x77A8F0, 0x77AB20, 0x77ADE0, 0x77B040, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x773BA0, 0x773BB0, 0x773BC0, 0x773C40, 0x773C70, 0x773CF0, 0x0, 0x774030, 0x774060, 0x774090, 0x7740A0, 0x7740B0, 0x7740C0, 0x7740D0, 0x0, 0x0, 0x774FD0, 0x0, 0x0, 0x774500, 0x774850, 0x0, 0x0, 0x0, 0x774370, 0x774F50, 0x774F90, 0x7766B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7792E0, 0x7793C0, 0x779560, 0x779630, 0x779750, 0x7799B0, 0x0, 0x0, 0x77BB90, 0x77BBE0, 0x0, 0x0, 0x77C4D0, 0x0, 0x0, 0x0, 0x77CEB0, 0x77CEE0, 0x77CFC0, 0x77D0E0, 0x77D230, 0x0, 0x77D8F0, 0x77DAD0, 0x77DC10, 0x7759E0, 0x0, 0x77F0D0, 0x77F110, 0x77F130, 0x77F150, 0x77F160, 0x77F1A0, 0x77F1B0, 0x77F1C0, 0x77F1F0, 0x77F200, 0x77F220 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x1557D28;        // pointer to the director block (phase +0x40 / requested +0x42 / next +0x44, frame +0x46, done +0x48)
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t PACKET_CURSOR = 0x25ACEB4;  // module packet cursor (tick parity: [0x25B394C] / [0x25B3948])
	static const uint32_t PLAY_PAUSE = 0x25AECD4;     // the plays' pause word (a_73BB60, with the switch word PAUSE_SWITCH)
	static const uint32_t PAUSE_SWITCH = 0x25B4B4C;   // the module's pause switch (a_73BB60: camera states 2 / 3, Angelo's state 2)
	static const uint32_t CREATURE_ARG = 0x25B3958;   // the creature renderer's state block (Angelo)
	static const uint32_t SCRATCH_SP = 0x25B4B24;     // module scratch stack pointer (the camera copy 0x2793E58)
	static const uint32_t ACTOR_STATE = 0x25B4B54;    // current actor system state block (pool header)
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; } // battle entity record of a slot
	// the task tail of the Boko text (finished, read before ++frame, and no child alive: released)
	static inline uint32_t b102_task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// engine functions not in act::x
	namespace gx
	{
		static inline uint32_t BindEntityModelFromComData(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x5022C0)(a1, a2, a3); } // BS_BindEntityModelFromComData (entity, 0x20-byte block, com data)
		static inline uint32_t QueueChainTransformation(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505C00)(a1, a2); } // (entity, chain transformation id)
		static inline uint32_t EntityChainLowest(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x501FF0)(a1); } // Battle_EntityChainLowest (entity record -> the lowest entity of its chain)
		static inline uint32_t BS_Camera_ArmReturnAfterEffect() { return fn<uint32_t (__cdecl *)()>(0x50A730)(); }
		static inline uint32_t SSIGPU_SetDrawEnv(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x45C0F0)(a1, a2); } // (prim, draw environment)
		static inline uint32_t GteReadScreenOffset(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56CD10)(a1, a2); } // sub_56CD10: OFX / OFY >> 16 into (int32 *x, int32 *y)
		static inline uint32_t GteSetScreenOffset(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56CCE0)(a1, a2); } // Call_Bs_ParseCamera (x, y)
		static inline uint32_t SetDrawAreaPrim(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x45C940)(a1, a2); } // sub_45C940 (prim, RECT16 *)
		static inline uint32_t SetDrawOffsetPrim(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x45C9B0)(a1, a2); } // sub_45C9B0 (prim, int16 xy[2])
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

	// the one-argument task tail of the Boko text (status read here)
	static inline uint32_t task_end(uint32_t node) { const uint8_t st = U8(node, 0x26); U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1); return task_end(node, st); }

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl ak_773A10(uint32_t a1);
	uint32_t __cdecl ak_773BC0(uint32_t a1);
	uint32_t __cdecl ak_773C00(void);
	uint32_t __cdecl ak_773CF0(uint32_t a1);
	uint32_t __cdecl ak_773D40();
	uint32_t __cdecl ak_774190(uint32_t a1);
	uint32_t __cdecl ak_774240(uint32_t a1);
	uint32_t __cdecl ak_7742E0(uint32_t a1);
	uint32_t __cdecl ak_774330(uint32_t a1);
	uint32_t __cdecl ak_7743A0(uint32_t a1);
	uint32_t __cdecl ak_774450(uint32_t a1);
	uint32_t __cdecl ak_7744B0(uint32_t a1);
	uint32_t __cdecl ak_774CB0(uint32_t a1);
	uint32_t __cdecl ak_774CE0(uint32_t a1);
	uint32_t __cdecl ak_774D40(uint32_t a1);
	uint32_t __cdecl ak_774D90(uint32_t a1);
	uint32_t __cdecl ak_774DE0(uint32_t a1);
	uint32_t __cdecl ak_774E20(uint32_t a1);
	uint32_t __cdecl ak_774E90(uint32_t a1);
	uint32_t __cdecl ak_774EC0(uint32_t a1);
	uint32_t __cdecl ak_774F10(uint32_t a1);
	uint32_t __cdecl ak_774F70(uint32_t a1);
	uint32_t __cdecl ak_775010(uint32_t a1);
	uint32_t __cdecl ak_7750E0(uint32_t a1);
	uint32_t __cdecl ak_775120(uint32_t a1);
	uint32_t __cdecl ak_775160(uint32_t a1);
	uint32_t __cdecl ak_7751A0(uint32_t a1);
	uint32_t __cdecl ak_775510(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_775720(uint32_t a1);
	uint32_t __cdecl ak_775780(uint32_t a1);
	uint32_t __cdecl ak_775E50(uint32_t a1);
	uint32_t __cdecl ak_775E70(uint32_t a1);
	uint32_t __cdecl ak_775F40(uint32_t a1);
	uint32_t __cdecl ak_776010(uint32_t a1);
	uint32_t __cdecl ak_7760E0(uint32_t a1);
	uint32_t __cdecl ak_776260(uint32_t a1);
	uint32_t __cdecl ak_7762D0(uint32_t a1);
	uint32_t __cdecl ak_7763B0(uint32_t a1);
	uint32_t __cdecl ak_776460(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl ak_776510(uint32_t a1);
	uint32_t __cdecl ak_7766B0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ak_776920(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_776A50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_776C70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_776F00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_777380(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_777E30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_778330(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_778980(uint32_t a1);
	uint32_t __cdecl ak_7789B0(uint32_t a1);
	uint32_t __cdecl ak_778A20(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_778AB0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_778E90(uint32_t a1);
	uint32_t __cdecl ak_778F90(uint32_t a1);
	uint32_t __cdecl ak_7790B0(uint32_t a1);
	uint32_t __cdecl ak_7792E0(uint32_t a1);
	uint32_t __cdecl ak_779560(void);
	uint32_t __cdecl ak_779C10(uint32_t a1);
	uint32_t __cdecl ak_779F80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_77A190(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_77A8F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_77AB20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_77ADE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_77B040(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ak_77B320(void);
	uint32_t __cdecl ak_77B3E0(uint32_t a1);
	uint32_t __cdecl ak_77B460(void);
	uint32_t __cdecl ak_77B4A0(uint32_t a1);
	uint32_t __cdecl ak_77C4D0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_77C5E0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_77C660(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_77C690(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ak_77C730(uint32_t a1);
	uint32_t __cdecl ak_77C7B0(uint32_t a1);
	uint32_t __cdecl ak_77C830(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ak_77D010(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_77D230(uint32_t a1);
	uint32_t __cdecl ak_77DD20(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_77DDB0(void);
	uint32_t __cdecl ak_77DE40(uint32_t a1);
	uint32_t __cdecl ak_77DE70(uint32_t a1);
	uint32_t __cdecl ak_77DFC0(uint32_t a1);
	uint32_t __cdecl ak_77E060(uint32_t a1);
	uint32_t __cdecl ak_77E180(uint32_t a1);
	uint32_t __cdecl ak_77E1E0(uint32_t a1);
	uint32_t __cdecl ak_77E210(uint32_t a1);
	uint32_t __cdecl ak_77E220(uint32_t a1);
	uint32_t __cdecl ak_77E260(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_77E490(uint32_t a1);
	uint32_t __cdecl ak_77E4D0(uint32_t a1);
	uint32_t __cdecl ak_77E600(uint32_t a1);
	uint32_t __cdecl ak_77E720(uint32_t a1);
	uint32_t __cdecl ak_77E780(uint32_t a1);
	uint32_t __cdecl ak_77E7D0(uint32_t a1);
	uint32_t __cdecl ak_77E800(uint32_t a1);
	uint32_t __cdecl ak_77E850(uint32_t a1);
	uint32_t __cdecl ak_77EAB0(uint32_t a1);
	uint32_t __cdecl ak_77EB70(uint32_t a1);
	uint32_t __cdecl ak_77EBD0(uint32_t a1);
	uint32_t __cdecl ak_77EC00(uint32_t a1);
	uint32_t __cdecl ak_77ECC0(uint32_t a1);
	uint32_t __cdecl ak_77ED10(uint32_t a1);
	uint32_t __cdecl ak_77ED50(uint32_t a1);
	uint32_t __cdecl ak_77EDF0(uint32_t a1);
	uint32_t __cdecl ak_77EE50(uint32_t a1);
	uint32_t __cdecl ak_77EEC0(uint32_t a1);
	uint32_t __cdecl ak_77EF90(uint32_t a1);
	uint32_t __cdecl ak_77EFD0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ak_77F030(uint32_t a1);
	uint32_t __cdecl ak_77F070(uint32_t a1);
	uint32_t __cdecl ak_77F0D0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag087_angelo_strike_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace strike87
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code matches a
	// Angelo Recover / Reverse / Rush / Search / Invincible Moon / Doom / Siren / Boko / Tonberry / The End port up to the module
	// addresses and callees (the port's text with the module's addresses; their comments describe the source)
	// ====================================================================================

	// 0x773A10 (MAG_087_sub_773A10): MASTER task - the engine's act-library master (a_739F40: camera matrix snapshot into
	// 0x2793E58 (pointers 0x25B4B24 / 0x25ACFC0), double-buffered packet arenas by tick parity (module cursor 0x25ACEB4 =
	// [0x25B394C] / [0x25B3948], second cursor 0x25ACFBC = [0x25B2AA4] / [0x25B2AA0]), target bone follow, the 11-state table
	// {0x773BA0 / 0x773BB0 (a_73A0D0), 0x773BC0, 0x773C40 (a_73A170), the library's finish states 0x77F160.., ret}, the nine
	// module queues) as the module's own port, with the held seam after the queues
	uint32_t __cdecl ak_773A10(uint32_t a1)
	{
		g_mod = &MOD_087;
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_master();)

		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x25B4B24) = 0x2793E58;
		MEM<uint32_t>(0x25ACFC0) = 0x2793E58;
		uint32_t states[11];
		states[0] = 0x773BA0;
		states[1] = 0x773BB0;
		states[2] = 0x773BC0;
		states[3] = 0x773C40;
		states[4] = 0x77F160;
		states[5] = 0x77F1A0;
		states[6] = 0x77F1B0;
		states[7] = 0x77F1C0;
		states[8] = 0x77F1F0;
		states[9] = 0x77F200;
		states[10] = 0x77F220; // nullsub (ret)
		if ((U8(node, 0x5C) & 1) != 0)
		{
			MEM<uint32_t>(0x25ACEB4) = MEM<uint32_t>(0x25B394C);
			MEM<uint32_t>(0x25ACFBC) = MEM<uint32_t>(0x25B2AA4);
		}
		else
		{
			MEM<uint32_t>(0x25ACEB4) = MEM<uint32_t>(0x25B3948);
			MEM<uint32_t>(0x25ACFBC) = MEM<uint32_t>(0x25B2AA0);
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(0x25B4B08) = 0;
		MEM<uint16_t>(0x25B4630) = 0;
		static const uint32_t queues[9] = { 0x25B37D8, 0x25ACF98, 0x25B4B10, 0x25B4620, 0x25B3538, 0x25AD0A0, 0x25AECD8, 0x25B2A90, 0x25B37C8 };
		for (int i = 0; i < 9; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_end();)
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x846850 (MAG_041_sub_846850; = Counter Laser-Eye 0x8738C0 without the stage light ramp):
	// master state - once no child is alive: the two actor pools are carved from the file arena
	// cursor [0x2698AB0] (0x870 bytes = 20 particles of 0x6C, 0x8340 bytes = 50 actors of 0x2A0)
	// and cleared (0x846890), next state
	uint32_t __cdecl ak_773BC0(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x25B4B50);
		MEM<uint32_t>(0x25B4634) = p;
		p += 0x870;
		MEM<uint32_t>(0x25B4B48) = p;
		p += 0x8340;
		MEM<uint32_t>(0x25B4B50) = p;
		ak_773C00();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7D6470 (MAG_071_sub_7D6470; = Wind Blast 0x879750 with this module's pool sizes): clears
	// the two actor pools (0x870 bytes at [0x25FCD8C], 0x8340 bytes at [0x25FCDC8]) and the pool
	// cursors / counters
	uint32_t __cdecl ak_773C00(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25B4634), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25B4B48), 0x8340);
		MEM<uint16_t>(0x25AECD0) = 0;
		MEM<uint16_t>(0x25B4640) = 0;
		MEM<uint16_t>(0x25B4444) = 0;
		MEM<uint16_t>(0x25ACEB8) = 0;
		return 0; // void
	}

	// 0x8C4E10 (copy of t_762710 0x762710: module 090 MAG_090_sub_762710): master state - init director data (t_762BB0 +
	// a_7322A0), spawns task 0x762850 (0x30 B, queue 0x25A4BF0) and the director task 0x762C60
	// (0x48 B, queue 0x259EF90), next state.
	uint32_t __cdecl ak_773CF0(uint32_t a1)
	{
		ak_774190(a1);
		ak_773D40();
		x::Effect_AddTaskAndInitFromCtx(0x25B4B10, 0x773E30, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x25ACF98, 0x774240, 0x48, a1);
		t1_next_state(a1);
		return 0; // void
	}

	// 0x773D40 (MAG_087_sub_773D40): the file loader block (0x25B4B58, 0x16C bytes; pointer 0x25B4448) and the load script cursor
	// 0x25B3950 = 0x155B190: +0x100 = an arena of 0xA000 bytes from the file arena cursor 0x25B4B50, +0x104.. the load
	// destinations (0x19CF018, +0x114 0x19EB018), +0x164 / +0x166 loading / script-done words cleared
	uint32_t __cdecl ak_773D40()
	{
		MEM<uint32_t>(0x25B3950) = 0x155B190;
		MEM<uint32_t>(0x25B4448) = 0x25B4B58;
		x::MAG_007_sub_8DCC00(0x25B4B58, 0x16C);
		const uint32_t b = MEM<uint32_t>(0x25B4448);
		uint32_t c = MEM<uint32_t>(0x25B4B50);
		U32(b, 0x100) = c;
		c += 0xA000;
		MEM<uint32_t>(0x25B4B50) = c;
		U32(b, 0x114) = 0x19EB018;
		for (int32_t o = 0x104; o <= 0x160; o += 4)
			if (o != 0x114) U32(b, o) = 0x19CF018;
		U16(b, 0x164) = 0;
		U16(b, 0x166) = 0;
		return b;
	}

	// 0x774190 (MAG_087_sub_774190): director data set-up - the target = the lowest entity of the target's chain (+0x2D,
	// Battle_EntityChainLowest 0x501FF0); clears the director block [DIR_PTR] (0x54 bytes), +8.. = the target's position (entity
	// +0x1C / +0x20), +0.. = the caster's (+0x2C), facing +0x4A = the angle from the caster to the target - 0x800 (12 bits), +0x4E =
	// the target's size (+0x26)
	uint32_t __cdecl ak_774190(uint32_t a1)
	{
		const uint32_t t = gx::EntityChainLowest(ENT(U8(a1, 0x2D)));
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(DIR_PTR), 0x54);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U32(d, 8) = U32(t, 0x1C);
		const uint32_t c = ENT(U8(a1, 0x2C));
		U32(d, 0xC) = U32(t, 0x20);
		U32(d, 0) = U32(c, 0x1C);
		U32(d, 4) = U32(c, 0x20);
		const int16_t dz = (int16_t)(U16(d, 0xC) - U16(d, 4));
		const int16_t dx = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx, (uint32_t)(int32_t)dz);
		const uint16_t size = U16(t, 0x26);
		const uint32_t d2 = MEM<uint32_t>(DIR_PTR);
		U16(d2, 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		U16(d2, 0x4E) = size;
		return d2;
	}

	// 0x774240 (MAG_087_sub_774240): TASK director (node 0x48) - phase bookkeeping (0x7742E0) every tick, then its 10 states
	// {0x774330 (TIM 0x16FDF78, camera return armed, the camera task, sync point 1), 0x774F70 (cue 1), 0x774FB0 (a_73A930: the
	// next load-script step), 0x775010 (from frame 0x1C: waits for the loads, pausing the module meanwhile), 0x7750A0 (a_73B8A0:
	// sync point 2), 0x7750C0 (a_73B8C0: cue 2), 0x7750E0 (done flag at frame 0x48), 0x775100 (a_733B70: sync point 3),
	// 0x775120 (cue 3 -> finished), ret}; frame +0x24; released once finished with no child alive
	uint32_t __cdecl ak_774240(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[10];
		states[0] = 0x774330;
		states[1] = 0x774F70;
		states[2] = 0x774FB0;
		states[3] = 0x775010;
		states[4] = 0x7750A0;
		states[5] = 0x7750C0;
		states[6] = 0x7750E0;
		states[7] = 0x775100;
		states[8] = 0x775120;
		states[9] = 0x775140; // nullsub (ret)
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_director(node);)
		ak_7742E0(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl ak_7742E0(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			ak_775160(a1);
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

	// 0x774330 (MAG_087_UploadAngeloTIM): director state 0 - queues Angelo's TIM 0x16FDF78, arms the camera return (0x50A730),
	// spawns the camera task 0x7743A0 (queue 0x25AD0A0, node 0x90); next state at director sync point 1 (a_73B640)
	uint32_t __cdecl ak_774330(uint32_t a1)
	{
		x::Battle_QueueTIMUpload_GetEOF(0x16FDF78);
		gx::BS_Camera_ArmReturnAfterEffect();
		x::Effect_AddTaskAndInitFromCtx(0x25AD0A0, 0x7743A0, 0x90, a1);
		if (a_73B640(1) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7148C0 (module 100 MAG_100_BokoCreatureTask): Boko creature script TASK - 14-state table
	// on +0x29 (camera/script steps), ++frame counter; ends when finished.
	uint32_t __cdecl ak_7743A0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[14];
		states[0] = 0x774450;
		states[1] = 0x7744B0;
		states[2] = 0x774CB0;
		states[3] = 0x774CE0;
		states[4] = 0x774D40;
		states[5] = 0x774D90;
		states[6] = 0x774DE0;
		states[7] = 0x774E20;
		states[8] = 0x774E90;
		states[9] = 0x774EC0;
		states[10] = 0x774EF0;
		states[11] = 0x774F10;
		states[12] = 0x774F30;
		states[13] = 0x774F60;
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_camera(node);)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b102_task_end(node, status);
	}

	// 0x774450 (sub_774450): camera state 0 - a_73AB00; the camera block [0x155B1E0] +0xE8 = the caster's (+0x2C) facing (entity
	// +0xE), next state
	uint32_t __cdecl ak_774450(uint32_t a1)
	{
		a_73AB00();
		const uint16_t ang = U16(ENT(U8(a1, 0x2C)), 0xE);
		U16(MEM<uint32_t>(0x155B1E0), 0xE8) = ang;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// the camera task's script chains (0x7743A0's states): when the running camera script ends (a_73AE10 returns 1) the next
	// script starts (a_73AB20) from the point a2, next state
	static inline void ak_cam_chain(uint32_t a1, uint32_t script, uint32_t from)
	{
		a_73AB20(script, from, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
	}

	// 0x769150 (sub_769150): camera state 1 - steps the camera script; at director sync point 1 (a_73AAE0) starts 0x1555D60
	uint32_t __cdecl ak_7744B0(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(1) != 0)
		{
			a_73AB20(0x15600F8, MEM<uint32_t>(DIR_PTR), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x774CB0 (sub_774CB0): camera state 2 - unless the module is paused (switch word 0x25B4B4C, a_73BB60): steps the camera
	// script; at its end starts 0x1560140 from the director block, next state
	uint32_t __cdecl ak_774CB0(uint32_t a1)
	{
		if (MEM<uint32_t>(0x25B4B4C) == 0 && a_73AE10() == 1)
			ak_cam_chain(a1, 0x1560140, MEM<uint32_t>(DIR_PTR));
		return 0; // void
	}

	// 0x774CE0 (sub_774CE0): camera state 3 - unless paused: steps the camera script; at its end starts 0x1560188 or 0x15601D0
	// (random bit 0) from Angelo's point (+0x148 of his node, director +0x18), next state
	uint32_t __cdecl ak_774CE0(uint32_t a1)
	{
		if (MEM<uint32_t>(0x25B4B4C) == 0 && a_73AE10() == 1)
		{
			const uint32_t angelo = U32(MEM<uint32_t>(DIR_PTR), 0x18);
			const uint32_t r = x::CrtRand();
			ak_cam_chain(a1, (r & 1) ? 0x1560188 : 0x15601D0, angelo + 0x148);
		}
		return 0; // void
	}

	// 0x774D40 (sub_774D40): camera state 4 - steps the camera script; at its end starts 0x1560218 (target size +0x4E < 0x1000)
	// or 0x1560260 from Angelo's point +0x148, next state
	uint32_t __cdecl ak_774D40(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			const uint32_t d = MEM<uint32_t>(DIR_PTR);
			const uint32_t angelo = U32(d, 0x18);
			ak_cam_chain(a1, S16(d, 0x4E) < 0x1000 ? 0x1560218 : 0x1560260, angelo + 0x148);
		}
		return 0; // void
	}

	// 0x774D90 (sub_774D90): camera state 5 - at the script's end starts 0x15602E0 (size < 0x1000) or 0x1560370 from Angelo's
	// point +0x148, next state
	uint32_t __cdecl ak_774D90(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			const uint32_t d = MEM<uint32_t>(DIR_PTR);
			const uint32_t angelo = U32(d, 0x18);
			ak_cam_chain(a1, S16(d, 0x4E) < 0x1000 ? 0x15602E0 : 0x1560370, angelo + 0x148);
		}
		return 0; // void
	}

	// 0x774DE0 (sub_774DE0): camera state 6 - at the script's end: 0x1560328 from Angelo's point +0x148 when the target size
	// +0x4E < 0x1000 (else the script stays ended), next state
	uint32_t __cdecl ak_774DE0(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			const uint32_t d = MEM<uint32_t>(DIR_PTR);
			if (S16(d, 0x4E) < 0x1000)
				a_73AB20(0x1560328, U32(d, 0x18) + 0x148, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x774E20 (sub_774E20): camera state 7 - at the script's end starts 0x15603B8 (size < 0x1000) or 0x1560400 from Angelo's
	// point +0x148 on the ground (y 0), next state
	uint32_t __cdecl ak_774E20(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			const uint32_t d = MEM<uint32_t>(DIR_PTR);
			const uint32_t angelo = U32(d, 0x18);
			alignas(4) uint8_t pt[8];
			U32(P(pt), 0) = U32(angelo, 0x148);
			U16(P(pt), 2) = 0;
			U32(P(pt), 4) = U32(angelo, 0x14C);
			ak_cam_chain(a1, S16(d, 0x4E) < 0x1000 ? 0x15603B8 : 0x1560400, P(pt));
		}
		return 0; // void
	}

	// 0x774E90 (sub_774E90): camera state 8 - at the script's end starts 0x1560468 from the director block, next state
	uint32_t __cdecl ak_774E90(uint32_t a1)
	{
		if (a_73AE10() == 1)
			ak_cam_chain(a1, 0x1560468, MEM<uint32_t>(DIR_PTR));
		return 0; // void
	}

	// 0x774EC0 (sub_774EC0): camera state 9 - at the script's end starts 0x15604F0 from the director block, next state
	uint32_t __cdecl ak_774EC0(uint32_t a1)
	{
		if (a_73AE10() == 1)
			ak_cam_chain(a1, 0x15604F0, MEM<uint32_t>(DIR_PTR));
		return 0; // void
	}

	// 0x763BD0 (module 090 sub_763BD0): director state - next state at director cue 3 (a_73AAE0).
	uint32_t __cdecl ak_774F10(uint32_t a1)
	{
		if (a_73AAE0(3) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x763B90 (module 090 sub_763B90): director state - next state at director cue 1 (a_73B7E0).
	uint32_t __cdecl ak_774F70(uint32_t a1)
	{
		if (a_73B7E0(1) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x775010 (sub_775010): director state 3 - from the director's frame +0x46 = 0x1C: while the loads run (loader block
	// [0x25B4448] +0x166) the module is paused (0x775050 = a_73BB60(1): the camera states 2 / 3, Angelo's state 2 and the plays
	// stop); when they are done a_73BB60(0), next state
	uint32_t __cdecl ak_775010(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x46) < 0x1C)
			return 0; // void
		if (U16(MEM<uint32_t>(0x25B4448), 0x166) == 0)
		{
			a_73BB60(0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		else
			a_73BB60(1);
		return 0; // void
	}

	// 0x7750E0 (sub_7750E0): director state 6 - from the director's frame +0x46 = 0x48: done flag +0x48 = 1 (the damage of the
	// library's finish states), next state
	uint32_t __cdecl ak_7750E0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		if (S16(d, 0x46) >= 0x48)
		{
			U16(d, 0x48) = 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x775120 (sub_775120): director state 8 - at director cue 3 (a_73B7E0): finished (+0x26 |= 1), next state
	uint32_t __cdecl ak_775120(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x775160 (MAG_087_SpawnAngeloCreature): the tasks of a new director phase ([DIR_PTR] +0x40): phase 1 = the ANGELO actor
	// 0x7751A0 (queue 0x25B4620, node 0x178) bound to the model container 0x155B1E4 animation 3
	uint32_t __cdecl ak_775160(uint32_t a1)
	{
		if (S16(MEM<uint32_t>(DIR_PTR), 0x40) == 1)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25B4620, 0x7751A0, 0x178, a1);
			x::Effect_BindModelContainerSetAnim(t, 0x155B1E4, 3);
		}
		return 0; // void
	}

	// 0x7751A0 (sub_7751A0): ANGELO actor task (node 0x178) - 17 states {0x776010 (binds Rinoa's animation set, her chain
	// transformation 1, saves the target's transform, faces it), 0x7760E0 (from frame 0xF: shown, runs at the target), 0x776260
	// (the run; stops while the module is paused), 0x7763B0 (the run ends: the first play 0x7764B0, the ground capture 0x7789B0,
	// sound), 0x778E90 (lift towards the target's point 0xF1), 0x778F90 (four actor systems 0x779050), 0x77DE70 (the throw: the
	// halo 0x77DFC0, the second dust play, carries the target), 0x77E490, 0x77E4D0 (spins), 0x77E600 (the slam: the third play
	// 0x77E6C0, the debris spawner 0x77E800, sound), 0x77ED50 (jump back to Rinoa), 0x77EDF0, 0x77EE50, 0x77EEC0 (the target's
	// transform put back, her chain transformation 2), 0x77EF90, 0x77F030 (turns away, hidden + finished), ret}; then velocity:
	// z +0x50 += +0x128, x +0x4C += +0x124, y +0x4E += +0x126; unless hidden (+0x26 bit 2) the model is drawn by the creature
	// renderer 0x775330 (a_746C10, state block 0x25B3958) into the module packet pool; then, on the animation frames that put a
	// foot down (animation +0x9C / frame +0xA2), the ring bursts 0x775510 / 0x775F40; frame +0x24; root matrix (+0x70) published
	// to 0x1D99AB0, position to +0x84/+0x88/+0x8C (s32); released once finished with no child alive
	uint32_t __cdecl ak_7751A0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[17];
		states[0] = 0x776010;
		states[1] = 0x7760E0;
		states[2] = 0x776260;
		states[3] = 0x7763B0;
		states[4] = 0x778E90;
		states[5] = 0x778F90;
		states[6] = 0x77DE70;
		states[7] = 0x77E490;
		states[8] = 0x77E4D0;
		states[9] = 0x77E600;
		states[10] = 0x77ED50;
		states[11] = 0x77EDF0;
		states[12] = 0x77EE50;
		states[13] = 0x77EEC0;
		states[14] = 0x77EF90;
		states[15] = 0x77F030;
		states[16] = 0x77F0C0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint16_t vz = U16(node, 0x128);
		const uint16_t vx = U16(node, 0x124);
		const uint16_t vy = U16(node, 0x126);
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + vz);
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) + vx);
		U16(node, 0x4E) = (uint16_t)(U16(node, 0x4E) + vy);
		if ((U8(node, 0x26) & 4) == 0)
		{
			// 30 fps layer: see mag087_angelo_strike_held.inc
			FX_HELD(held_note_draw(HK_ANGELO, node);)
			MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, CREATURE_ARG, MEM<uint32_t>(PACKET_CURSOR));
			if ((U8(node, 0x26) & 4) == 0)
			{
				const uint8_t anim = U8(node, 0x9C), frame = U8(node, 0xA2);
				if (anim == 1)
				{
					if (frame <= 5)
						ak_775F40(node);
					else if (frame >= 0x10)
						ak_775510(node, (frame & 1) ? 2 : 3);
				}
				else if (anim == 8)
				{
					if (frame <= 5)
						ak_775F40(node);
				}
			}
		}
		const int32_t py = S16(node, 0x4E);
		const int32_t px = S16(node, 0x4C);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		memcpy((void *)0x1D99AB0, (const void *)(node + 0x70), 8 * 4); // rep movsd
		const int32_t pz = S16(node, 0x50);
		S32(node, 0x88) = py;
		const uint8_t status = U8(node, 0x26);
		S32(node, 0x84) = px;
		S32(node, 0x8C) = pz;
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x775510 (sub_775510): a ring burst at foot point a2 (0..3 -> spawn points 0x1E / 0x21 / 0x1C / 0x1D of Angelo's model +0x30;
	// 4 / 5 -> 0x1C / 0x1D): a2 0..3 = four ring particles 0x775720 (queue 0x25AD0A0, node 0x90: on the ground, +-0x80 x / z,
	// velocity +0x78.. = the vector (0, 0, 0x200..0x11FF) turned by a random yaw +-0x200, rise -(0..0x5F) per tick +0x82); a2 4 / 5
	// = two ring particles at the point (+-0x40 x / z, height scaled by 0xC0..0xFF / 256, velocity 0 turned, rise -(0..0x5F))
	uint32_t __cdecl ak_775510(uint32_t a1, uint32_t a2)
	{
		static const uint32_t PTS[6] = { 0x1E, 0x21, 0x1C, 0x1D, 0x1C, 0x1D };
		const uint32_t mdl = a1 + 0x30;
		const int16_t k = (int16_t)a2;
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		const int n = k > 3 ? 2 : 4;
		for (int i = 0; i < n; i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25AD0A0, 0x775720, 0x90, a1);
			if ((uint32_t)(int32_t)k <= 5)
				x::GetEffectSpawnPosition(mdl, PTS[k], 0, t + 0x1C);
			if (k <= 3)
			{
				const int32_t r1 = (int32_t)x::CrtRand();
				U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((r1 & 0xFF) - 0x80));
				const int32_t r2 = (int32_t)x::CrtRand();
				U16(t, 0x1E) = 0;
				U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((r2 & 0xFF) - 0x80));
				x::MAG_022_sub_8DD770(M);
				const int32_t r3 = (int32_t)x::CrtRand();
				x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(((r3 & 0x3FF) - 0x200) & 0xFFF));
				const int32_t r4 = (int32_t)x::CrtRand();
				U16(t, 0x78) = 0;
				U16(t, 0x7A) = 0;
				U16(t, 0x82) = (uint16_t)(-(r4 % 0x60));
				const int32_t r5 = (int32_t)x::CrtRand();
				U16(t, 0x7C) = (uint16_t)((r5 & 0xFFF) + 0x200);
			}
			else
			{
				const int32_t r1 = (int32_t)x::CrtRand();
				U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((r1 & 0x7F) - 0x40));
				const int32_t r2 = (int32_t)x::CrtRand();
				U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((r2 & 0x7F) - 0x40));
				const int32_t r3 = (int32_t)x::CrtRand();
				const int32_t f = (int32_t)(int16_t)((r3 & 0x3F) + 0xC0);
				U16(t, 0x1E) = (uint16_t)((int32_t)S16(t, 0x1E) * f / 256);
				x::MAG_022_sub_8DD770(M);
				const int32_t r4 = (int32_t)x::CrtRand();
				x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(((r4 & 0x3FF) - 0x200) & 0xFFF));
				const int32_t r5 = (int32_t)x::CrtRand();
				U16(t, 0x78) = 0;
				U16(t, 0x7A) = 0;
				U16(t, 0x7C) = 0;
				U16(t, 0x82) = (uint16_t)(-(r5 % 0x60));
			}
			x::matrixMultiplyVector(M, t + 0x78, t + 0x78);
		}
		return 0; // void
	}

	// 0x767290 (module 090 sub_767290): ring particle task (states 0x767410/0x767430) drawn by
	// ak_775780; ends when finished and no child alive.
	uint32_t __cdecl ak_775720(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x775E50;
		states[1] = 0x775E70;
		states[2] = 0x775F30;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		ak_775780(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8CD770 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl ak_775780(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag087_angelo_strike_held.inc
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
		const uint32_t cursor = MEM<uint32_t>(0x25ACEB4);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x25ACEB4) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x7618E0 (sub_7618E0): ring particle state 0 - prim sequence 0x15430E8, colour +0x56 = 0xFFC0, +0x52 = 7, next state
	uint32_t __cdecl ak_775E50(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x1557D34;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 7;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x761900 (sub_761900): ring particle state 1 - sequence step (0x761990 = a_743C20; at its end hidden + finished, next
	// state); y velocity +0x7A += +0x82, x / z velocity damped by 1/8, position += velocity / 16
	uint32_t __cdecl ak_775E70(uint32_t a1)
	{
		const uint32_t n = a1;
		if (a_743C20(n) != 0)
		{
			U8(n, 0x26) = (uint8_t)(U8(n, 0x26) | 5);
			U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
		}
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
		return 0; // void
	}

	// 0x775F40 (sub_775F40): one ring particle 0x775720 (queue 0x25AD0A0, node 0x90) at Angelo's point 0xF1 on the ground, +-0x80
	// x / z, velocity +0x78.. = the vector (0, 0, 0x200..0x9FF) turned by a random yaw, rise -(0..0x5F) per tick +0x82
	uint32_t __cdecl ak_775F40(uint32_t a1)
	{
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25AD0A0, 0x775720, 0x90, a1);
		x::GetEffectSpawnPosition(a1 + 0x30, 0xF1, 0, t + 0x1C);
		U16(t, 0x1E) = 0;
		const int32_t r1 = (int32_t)x::CrtRand();
		U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((r1 & 0xFF) - 0x80));
		const int32_t r2 = (int32_t)x::CrtRand();
		U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((r2 & 0xFF) - 0x80));
		x::MAG_022_sub_8DD770(M);
		const int32_t r3 = (int32_t)x::CrtRand();
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)(r3 & 0xFFF));
		const int32_t r4 = (int32_t)x::CrtRand();
		U16(t, 0x78) = 0;
		U16(t, 0x7A) = 0;
		U16(t, 0x82) = (uint16_t)(-(r4 % 0x60));
		const int32_t r5 = (int32_t)x::CrtRand();
		U16(t, 0x7C) = (uint16_t)((r5 & 0x7FF) + 0x200);
		x::matrixMultiplyVector(M, t + 0x78, t + 0x78);
		return 0; // void
	}

	// 0x776010 (sub_776010): Angelo state 0 - the director's +0x18 = this node; binds Rinoa's animation set (com data 0x1560550,
	// 0x20-byte block 0x25B4B28) to the caster (+0x2C, BS_BindEntityModelFromComData 0x5022C0), queues her chain transformation 1
	// (QueueChainTransformation 0x505C00); hidden; at the caster's position (director +0..), facing +0x3E = director +0x4A; the
	// target (the lowest entity of the +0x2D chain) turned to face him (entity +0xE = facing - 0x800) after its rotation +0xC.. /
	// position +0x1C.. were kept in +0x150.. / +0x158..; scale 0x1000^3, next state
	uint32_t __cdecl ak_776010(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t c = ENT(U8(node, 0x2C));
		const uint32_t t = gx::EntityChainLowest(ENT(U8(node, 0x2D)));
		U32(MEM<uint32_t>(DIR_PTR), 0x18) = node;
		gx::BindEntityModelFromComData(c, 0x25B4B28, 0x1560550);
		gx::QueueChainTransformation(c, 1);
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 4);
		const uint32_t rot0 = U32(t, 0xC);
		U32(node, 0x4C) = U32(d, 0);
		const uint16_t face = U16(d, 0x4A);
		U16(node, 0x3E) = face;
		U32(node, 0x50) = U32(d, 4);
		const uint32_t rot1 = U32(t, 0x10);
		U16(t, 0xE) = (uint16_t)((face - 0x800) & 0xFFF);
		U32(node, 0x11C) = 0x1000;
		U32(node, 0x118) = 0x1000;
		U32(node, 0x114) = 0x1000;
		U32(node, 0x150) = rot0;
		U32(node, 0x154) = rot1;
		U32(node, 0x158) = U32(t, 0x1C);
		U32(node, 0x15C) = U32(t, 0x20);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7760E0 (sub_7760E0): Angelo state 1 - from frame 0xF: the run's end point +0x148.. = the target's position + (0, 0, half
	// the target's radius +0x38 - 0x400) turned by the target's facing (entity +0xE), its start +0x140.. = that end + (0, 0,
	// -0x2000) turned likewise; shown at the start, the run's factor +0x168 = 0 (speed +0x16A = 0xBB), height +0x16E = 0 (rising
	// at +0x170 = -0x200, gravity +0x172 = 0x2E), animation 0, next state
	uint32_t __cdecl ak_7760E0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t t = gx::EntityChainLowest(ENT(U8(node, 0x2D)));
		if (S16(node, 0x24) < 0xF)
			return 0; // void
		// stack: +0 SVECTOR (+6 pad unwritten, never read), +8 the rotation matrix
		alignas(4) uint8_t loc[0x28];
		const uint32_t V = P(loc), M = V + 8;
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(t, 0xE));
		U16(V, 4) = (uint16_t)((int32_t)S16(t, 0x38) / 2 - 0x400);
		U16(V, 0) = 0;
		U16(V, 2) = 0;
		x::matrixMultiplyVector(M, V, V);
		U32(node, 0x148) = U32(t, 0x1C);
		U32(node, 0x14C) = U32(t, 0x20);
		U16(node, 0x148) = (uint16_t)(U16(node, 0x148) + U16(V, 0));
		U16(node, 0x14C) = (uint16_t)(U16(node, 0x14C) + U16(V, 4));
		U16(node, 0x14A) = U16(V, 2);
		U16(V, 0) = 0;
		U16(V, 2) = 0;
		U16(V, 4) = 0xE000;
		x::matrixMultiplyVector(M, V, V);
		U32(node, 0x140) = U32(node, 0x148);
		U8(node, 0x26) = (uint8_t)(U8(node, 0x26) & 0xFB);
		U32(node, 0x144) = U32(node, 0x14C);
		U16(node, 0x140) = (uint16_t)(U16(node, 0x140) + U16(V, 0));
		U16(node, 0x142) = (uint16_t)(U16(node, 0x142) + U16(V, 2));
		U16(node, 0x144) = (uint16_t)(U16(node, 0x144) + U16(V, 4));
		U32(node, 0x4C) = U32(node, 0x140);
		U32(node, 0x50) = U32(node, 0x144);
		U16(node, 0x168) = 0;
		U16(node, 0x16A) = 0xBB;
		U16(node, 0x16C) = 0;
		U16(node, 0x16E) = 0;
		U16(node, 0x170) = 0xFE00;
		U16(node, 0x172) = 0x2E;
		x::au_re_Battle_ReadAnimation_7(node, 0);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// Angelo's height step (inlined in his states 2 / 3 / 11 / 12 / 13): velocity +0x170 += gravity +0x172, height +0x16E += it,
	// kept <= 0
	static inline void ak_height_step(uint32_t node)
	{
		U16(node, 0x170) = (uint16_t)(U16(node, 0x170) + U16(node, 0x172));
		U16(node, 0x16E) = (uint16_t)(U16(node, 0x16E) + U16(node, 0x170));
		if (S16(node, 0x16E) > 0)
			U16(node, 0x16E) = 0;
	}

	// 0x776260 (sub_776260): Angelo state 2 - unless the module is paused (0x25B4B4C): height step, the run (0x7762D0), animation
	// step (0x8DD1C0); from animation frame +0x136 = 0x14 animation 0xA, next state
	uint32_t __cdecl ak_776260(uint32_t a1)
	{
		if (MEM<uint32_t>(0x25B4B4C) != 0)
			return 0; // void
		const uint32_t node = a1;
		ak_height_step(node);
		ak_7762D0(node);
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) >= 0x14)
		{
			x::au_re_Battle_ReadAnimation_7(node, 0xA);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7762D0 (sub_7762D0): Angelo's run - speed +0x16A += +0x16C, factor +0x168 += it; at 0x1000 the position = the end
	// (+0x148..); else from +0x140.. towards +0x148.. by factor / 0x1000 (each axis truncated); y += the height +0x16E
	uint32_t __cdecl ak_7762D0(uint32_t a1)
	{
		const uint32_t n = a1;
		U16(n, 0x16A) = (uint16_t)(U16(n, 0x16A) + U16(n, 0x16C));
		U16(n, 0x168) = (uint16_t)(U16(n, 0x168) + U16(n, 0x16A));
		const int16_t f = S16(n, 0x168);
		if (f >= 0x1000)
		{
			U32(n, 0x4C) = U32(n, 0x148);
			U32(n, 0x50) = U32(n, 0x14C);
			U16(n, 0x168) = 0x1000;
			U16(n, 0x4E) = (uint16_t)(U16(n, 0x4E) + U16(n, 0x16E));
			return n;
		}
		const int32_t k = f;
		for (uint32_t o = 0; o < 6; o += 2)
		{
			const int16_t a = S16(n, 0x140 + o);
			U16(n, 0x4C + o) = (uint16_t)(((int32_t)S16(n, 0x148 + o) - a) * k / 4096 + a);
		}
		U16(n, 0x4E) = (uint16_t)(U16(n, 0x4E) + U16(n, 0x16E));
		return n;
	}

	// 0x7763B0 (sub_7763B0): Angelo state 3 - height step, the run (0x7762D0); when the animation ends (0x8DD220): animation 1, the
	// first dust play 0x7764B0 (layout 0x16FD248 / 0xB0, 0x776460; +0x240 = his model node + 0x30, +0x8A = 0), the ground
	// capture 0x7789B0, the sound 0x1557D2C, next state
	uint32_t __cdecl ak_7763B0(uint32_t a1)
	{
		const uint32_t node = a1;
		ak_height_step(node);
		ak_7762D0(node);
		const uint32_t r = x::au_re_Battle_ReadAnimation_8(node);
		if (r == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, r);
			const uint32_t t = ak_776460(node, 0x7764B0, 0x16FD248, 0xB0, 0, 0);
			U32(t, 0x240) = node + 0x30;
			U16(t, 0x8A) = 0;
			ak_7789B0(node);
			x::BdPlaySE(0x1557D2C, 0, 0x80);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x776460 (sub_776460): spawns the play task a2 (queue 0x25B2A90, node 0x25C) from context a1: +0x74 = a3 (prim layout),
	// +0x78 = (s16) a4 (its count), +0x80 = a5 (the callback's mode word), +0x82 = a6; returns the node
	uint32_t __cdecl ak_776460(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25B2A90, a2, 0x25C, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x776510 (sub_776510): dust play state 0 (task 0x7764B0 = a_73A380) - decodes the prim layout (+0x74, count +0x78) into
	// +0x94, placed at Angelo's point 0xF1 (model +0x240) on the ground, scale 0xC00^3 (+0x8A = 0) or 0x1000^3, plays the first
	// frame (0x776580 = a_73C280), next state
	uint32_t __cdecl ak_776510(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t mdl = U32(node, 0x240);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(mdl, 0xF1, 0, node + 0x1C);
		const int32_t s = U16(node, 0x8A) != 0 ? 0x1000 : 0xC00;
		U16(node, 0x1E) = 0;
		S32(node, 0x58) = s;
		S32(node, 0x54) = s;
		S32(node, 0x50) = s;
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7766B0 (sub_7766B0): prim-model player draw callback of the module's plays (layout a1, record a2, block a3) - as Angelo
	// Search's 0x758EA0 (vertex frame lerped by MAG_017_sub_701390 into the block's +0x48 buffer; depth offset block +0x58; scale
	// words 0x100) with: record flag 0x200 = the position as is (no parent rotation, no matrix product), else the parent-rotated
	// position and the parent's rotation applied; flags 0x2030 (record flag 0x4000: 0x2000), | 0xC0 with alpha + colour; drawn
	// with 0x776920 into OT base+0x44 (shift 2) at the module packet cursor 0x25ACEB4
	uint32_t __cdecl ak_7766B0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position (+6 pad unwritten), +8 scale (3 x int32), +0x18 Mat4x3 object matrix (+0x2C translation)
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
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		U16(V, 0) = vx;
		U16(V, 2) = vy;
		U16(V, 4) = vz;
		if ((U8(rec, 5) & 2) != 0)
		{
			// record flag 0x200: the position as is (no parent rotation, no matrix product)
			S32(M, 0x14) = (int16_t)vx;
			S32(M, 0x18) = (int16_t)vy;
			S32(M, 0x1C) = (int16_t)vz;
		}
		else
		{
			x::GTE_SetRotMatrix(blk);
			x::GTE_LoadV0(V);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			x::GTE_MatrixMultiply(blk, M);
		}
		S32(M, 0x14) = add32(S32(M, 0x14), S32(blk, 0x14));
		S32(M, 0x18) = add32(S32(M, 0x18), S32(blk, 0x18));
		S32(M, 0x1C) = add32(S32(M, 0x1C), S32(blk, 0x1C));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			S32(S, 0) = S16(rec, 0x18);
			S32(S, 4) = S16(rec, 0x1A);
			S32(S, 8) = S16(rec, 0x1C);
			x::scale3DMatrix(M, S);
		}
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		U32(hdr, 0x14) = (U32(rec, 4) & 0x4000) != 0 ? 0x2000 : 0x2030;
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			U32(hdr, 0x14) = U32(hdr, 0x14) | 0xC0;
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
		MEM<uint32_t>(PACKET_CURSOR) = ak_776920(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 ak_776F00, FT4 ak_777380,
	// a_73D770, a_73D9C0, 0x73DCA0, ak_778330) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl ak_776920(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = ak_776A50(ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ak_776C70(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ak_776F00(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ak_777380(ctx, a2, a3, cur);
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
			cur = ak_777E30(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return ak_778330(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl ak_776A50(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_776C70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_776F00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_777380(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_777E30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_778330(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl ak_778980(uint32_t a1)
	{
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		const uint32_t r = a_73C280(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x7789B0 (sub_7789B0): the ground capture of Angelo's dig - the bounding box of the model 0x155AA5C (0x778A20) and the
	// capture 0x778AB0 around his position (+0x4C / +0x50, y 0)
	uint32_t __cdecl ak_7789B0(uint32_t a1)
	{
		// stack: +0 the anchor SVECTOR (x, 0, z, pad = his +0x52), +8 the box (min x / y / z, max x / y / z)
		alignas(4) uint8_t loc[0x14];
		const uint32_t Q = P(loc), B = Q + 8;
		U16(B, 4) = 0x7FFF;
		U16(B, 2) = 0x7FFF;
		U16(B, 0) = 0x7FFF;
		U16(B, 0xA) = 0x8001;
		U16(B, 8) = 0x8001;
		U16(B, 6) = 0x8001;
		ak_778A20(0x155AA5C, B);
		U32(Q, 0) = U32(a1, 0x4C);
		U32(Q, 4) = U32(a1, 0x50);
		U16(Q, 2) = 0;
		return ak_778AB0(Q, B);
	}

	// 0x778A20 (sub_778A20): the bounding box a2 (min x / y / z +0.., max +6..) of the vertices of the model a1 (count +4, 8-byte
	// vertices from +0xC; every vertex after the first tested, x / y / z each against the min, else the max)
	uint32_t __cdecl ak_778A20(uint32_t a1, uint32_t a2)
	{
		int32_t n = (int32_t)U32(a1, 4) - 1;
		uint32_t v = a1 + 0xC;
		const uint32_t b = a2;
		U16(b, 6) = U16(v, 0);
		U16(b, 0) = U16(v, 0);
		U16(b, 8) = U16(v, 2);
		U16(b, 2) = U16(v, 2);
		U16(b, 0xA) = U16(v, 4);
		U16(b, 4) = U16(v, 4);
		do
		{
			v += 8;
			for (uint32_t o = 0; o < 6; o += 2)
			{
				const int16_t c = S16(v, o);
				if (S16(b, o) > c)
					U16(b, o) = (uint16_t)c;
				else if (S16(b, 6 + o) < c)
					U16(b, 6 + o) = (uint16_t)c;
			}
		} while (--n != 0);
		return a2;
	}

	// 0x778AB0 (sub_778AB0): the ground capture - the scene around the anchor a1 (box a2) rendered into the 64 x 64 texture at
	// (0x300, 0x180): draw environment of the other buffer, the stage model 0x1D989C0 (when shown) seen from above (a scaled
	// matrix with its translation at the GTE projection distance 0x1D8E038, screen offset at the texture's centre, put back after),
	// a black semi-transparent quad and a draw mode, a black quad, then draw area / offset back to the texture rectangle; every
	// packet from the frame arena 0x1D8E054 into the effect OT (base + 0x44)
	uint32_t __cdecl ak_778AB0(uint32_t a1, uint32_t a2)
	{
		const uint32_t box = a2;
		const int32_t minx = -(int32_t)S16(box, 0), maxx = S16(box, 6), minz = -(int32_t)S16(box, 4), maxz = S16(box, 0xA);
		const int32_t width = maxx + minx, depth = maxz + minz;
		alignas(4) int16_t rect[4] = { 0x300, 0x180, 0x40, 0x40 };
		int32_t sx = width / 64;
		int32_t sz = depth / 64;
		if (sx % 64) sx++;
		if (sz % 64) sz++;
		// Mat4x3 (3x3 s16 + pad, translation at +0x14) and the scale vector
		alignas(4) uint8_t mbuf[0x20];
		const uint32_t M = P(mbuf);
		alignas(4) int32_t sv[3];
		x::MAG_022_sub_8DD770(M);
		x::sub_8DD7E0(M, 0x400);
		S32(M, 0x14) = (minx - maxx) / 2 / sx - (int32_t)S16(a1, 0) / sx;
		const int32_t tz = (minz - maxz) / 2 / sz;
		const int32_t az = (int32_t)S16(a1, 4) / sz;
		S32(M, 0x1C) = (int32_t)MEM<int16_t>(0x1D8E038);
		sv[1] = 0;
		S32(M, 0x18) = az - tz;
		sv[0] = 0x1000000 / ((int32_t)shl32(width, 12) / rect[2]);
		sv[2] = 0x1000000 / ((int32_t)shl32(depth, 12) / rect[3]);
		x::scale3DMatrix(M, P(sv));
		const uint32_t env = MEM<uint32_t>(0x1D8E054);
		MEM<uint32_t>(0x1D8E054) = env + 0x40;
		gx::SSIGPU_SetDrawEnv(env, 0x1D969C8 + 0x5C * (uint32_t)(MEM<uint8_t>(0x1D96A80) ^ 1));
		x::SSIGPU_InsertPrimAutoDepth(MEM<uint32_t>(0x1D8E04C) + 0x44, env);
		alignas(4) int32_t ofs[2] = {}; // [1] = x, [0] = y (sub_56CD10's two outputs)
		gx::GteReadScreenOffset(P(&ofs[1]), P(&ofs[0]));
		gx::GteSetScreenOffset((uint32_t)(int32_t)(rect[2] / 2), (uint32_t)(int32_t)(rect[3] / 2));
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		if ((MEM<uint8_t>(0x1D989BD) & 1) != 0)
		{
			// render header (Field_Alloc 0x2C; +0 / +8 / +0xC / +0x28 never written: the scratch's own words)
			const uint32_t h = x::Field_Alloc(0x2C);
			U16(h, 0x24) = MEM<uint16_t>(0x1D989BE);
			U16(h, 0x14) = 0;
			U16(h, 0x16) = 0;
			U16(h, 0x18) = 0x140;
			U16(h, 0x1A) = 0xD8;
			U32(h, 0x1C) = MEM<uint32_t>(0x1D989E4);
			U32(h, 4) = MEM<uint32_t>(0x1D98B3C);
			U32(h, 0x20) = 0xFFFFFFFF;
			U32(h, 0x10) = MEM<uint32_t>(0x1D969A8);
			x::BattleModel_BuildBoneMatricesFromPose(0x1D989D0);
			x::BS_ComputeBonesWorldMatrices(0x1D989D0, M);
			MEM<uint32_t>(0x1D8E054) = x::RenderGeometry(0x1D989C0, h, MEM<uint32_t>(0x1D8E04C) + 0x44, 0x10, MEM<uint32_t>(0x1D8E054));
			x::Field_Free(0x2C);
		}
		gx::GteSetScreenOffset((uint32_t)ofs[1], (uint32_t)ofs[0]);
		// two black quads (POLY_F4: 0x2A semi-transparent, then 0x28) over the 64 x 64 rectangle, around a draw mode
		auto quad = [](uint8_t code) {
			const uint32_t pk = MEM<uint32_t>(0x1D8E054);
			MEM<uint32_t>(0x1D8E054) = pk + 0x18;
			U32(pk, 0) = 0x5000000;
			U8(pk, 7) = code;
			U16(pk, 0x10) = 0;
			U16(pk, 8) = 0;
			U16(pk, 0x14) = 0x3F;
			U16(pk, 0xC) = 0x3F;
			U16(pk, 0xE) = 0;
			U16(pk, 0xA) = 0;
			U16(pk, 0x16) = 0x3F;
			U16(pk, 0x12) = 0x3F;
			U8(pk, 6) = 0;
			U8(pk, 5) = 0;
			U8(pk, 4) = 0;
			x::SSIGPU_InsertPrimAltViewport(MEM<uint32_t>(0x1D8E04C) + 0x44, pk);
		};
		quad(0x2A);
		const uint32_t dm = MEM<uint32_t>(0x1D8E054);
		MEM<uint32_t>(0x1D8E054) = dm + 0xC;
		x::sub_45BFC0(dm, 0, 0, x::sub_45C690(0, 1, 0, 0) & 0xFFFF, 0);
		x::SSIGPU_InsertPrimAutoDepth(MEM<uint32_t>(0x1D8E04C) + 0x44, dm);
		quad(0x28);
		const uint32_t area = MEM<uint32_t>(0x1D8E054);
		MEM<uint32_t>(0x1D8E054) = area + 0xC;
		gx::SetDrawAreaPrim(area, P(rect));
		x::SSIGPU_InsertPrimAutoDepth(MEM<uint32_t>(0x1D8E04C) + 0x44, area);
		const uint32_t ofp = MEM<uint32_t>(0x1D8E054);
		MEM<uint32_t>(0x1D8E054) = ofp + 0xC;
		alignas(4) int16_t xy[2] = { rect[0], rect[1] }; // (the original keeps them in its first argument's stack slot)
		gx::SetDrawOffsetPrim(ofp, P(xy));
		x::SSIGPU_InsertPrimAutoDepth(MEM<uint32_t>(0x1D8E04C) + 0x44, ofp);
		return 0; // void
	}

	// 0x778E90 (sub_778E90): Angelo state 4 - when the animation ends (0x8DD220): animation 2, the lift: from his position
	// (+0x140..) to the target's point 0xF1 (+0x148..) + (0, 0x200, half the target's radius +0x38) turned by the target's facing
	// (entity +0xE), factor +0x168 = 0 (speed +0x16A = 0x801, +0x16C = 0), next state
	uint32_t __cdecl ak_778E90(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t t = gx::EntityChainLowest(ENT(U8(node, 0x2D)));
		if (x::au_re_Battle_ReadAnimation_8(node) != 1)
			return 0; // void
		x::au_re_Battle_ReadAnimation_7(node, 2);
		U32(node, 0x140) = U32(node, 0x4C);
		U32(node, 0x144) = U32(node, 0x50);
		x::GetEffectSpawnPosition(t, 0xF1, 0, node + 0x148);
		// stack: +0 SVECTOR (+6 pad unwritten, never read), +8 the rotation matrix
		alignas(4) uint8_t loc[0x28];
		const uint32_t V = P(loc), M = V + 8;
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(t, 0xE));
		U16(V, 4) = (uint16_t)((int32_t)S16(t, 0x38) / 2);
		U16(V, 0) = 0;
		U16(V, 2) = 0x200;
		x::matrixMultiplyVector(M, V, V);
		U16(node, 0x14A) = (uint16_t)(U16(node, 0x14A) + U16(V, 2));
		U16(node, 0x148) = (uint16_t)(U16(node, 0x148) + U16(V, 0));
		U16(node, 0x14C) = (uint16_t)(U16(node, 0x14C) + U16(V, 4));
		U16(node, 0x168) = 0;
		U16(node, 0x16A) = 0x801;
		U16(node, 0x16C) = 0;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x778F90 (sub_778F90): Angelo state 5 - the lift (0x7762D0); at animation frame +0x136 = 1 the four actor systems 0x779050
	// (a_73C100: data 0x16FBE40, 0 / 5 / 4; +0x2A0 = 0..3 = the foot point each follows); when the animation ends (0x8DD220)
	// animation 3, next state
	uint32_t __cdecl ak_778F90(uint32_t a1)
	{
		const uint32_t node = a1;
		ak_7762D0(node);
		if (U16(node, 0x136) == 1)
		{
			for (uint32_t i = 0; i < 4; i++)
			{
				const uint32_t t = a_73C100(node, 0x779050, 0x16FBE40, 0, 5, 4);
				U16(t, 0x2A0) = (uint16_t)i;
			}
		}
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 3);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7790B0 (sub_7790B0): actor system state 0 (task 0x779050 = a_73A380) - from its frame +0x24 = its delay +0x298: the
	// emitter point +0x290.. = Angelo's (director +0x18) spawn point 0x1E / 0x21 / 0x1C / 0x1D by +0x2A0, the system set up
	// (0x779130 = a_73F990) and stepped once (0x779C10), next state
	uint32_t __cdecl ak_7790B0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < S16(node, 0x298))
			return 0; // void
		const uint32_t mdl = U32(MEM<uint32_t>(DIR_PTR), 0x18) + 0x30;
		const int16_t k = S16(node, 0x2A0);
		const int32_t pt = k == 0 ? 0x1E : k == 1 ? 0x21 : (k != 2 ? 0x1D : 0x1C);
		x::GetEffectSpawnPosition(mdl, (uint32_t)pt, 0, node + 0x290);
		a_73F990(node);
		ak_779C10(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl ak_7792E0(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x25B4B54);
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
	uint32_t __cdecl ak_779560(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x25B4B54);
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
	uint32_t __cdecl ak_779C10(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x25B4B54, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			ak_77DDB0();
			ak_77B460();
			// 30 fps layer: see mag087_angelo_strike_held.inc
			FX_HELD(held_note_system(sys);)
			ak_77B320();
			a_735440();
			sys = U32(0x25B4B54, 0);
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
		U16(0x25B4B08, 0) = (uint16_t)(U16(0x25B4B08, 0) + c14);
		U16(0x25B4630, 0) = (uint16_t)(U16(0x25B4630, 0) + c16);
		return done;
	}

	// 0x72BA00 (097 sub_72BA00, 098 0x723800, 099 0x719D50, 100 0x7109D0): flat-shaded triangle
	// mesh emitter - for each 0xC-byte face at ctx +0x20 (count first) projects its 3 vertices
	// (ctx +4 base, word indices *4), builds a POLY_F3 packet (0x14 bytes) at a4, culls (GTE flag,
	// backface unless ctx+0x1C bit 0x10, screen range), optionally light-colours it (ctx+0x1C bit
	// 0x40) and inserts it into OT a2 at OTZ >> a3; returns the new packet cursor (never runs in
	// the harness)
	uint32_t __cdecl ak_779F80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_77A190(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_77A8F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_77AB20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_77ADE0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_77B040(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ak_77B320(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x25B4B54), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x25B4B54);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						ak_77B3E0(act);   // (the original also pushes the bone entry, unused)
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
							ak_77B3E0(act);
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
	uint32_t __cdecl ak_77B3E0(uint32_t a1)
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
	uint32_t __cdecl ak_77B460(void)
	{
		uint32_t act = U32(U32(0x25B4B54, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				ak_77C730(act);
			else if (type == 1)
				ak_77B4A0(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x848EF0 (copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl ak_77B4A0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x25B4B54);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (ak_77C5E0(act, bone) == 0)
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
			st = MEM<uint32_t>(0x25B4B54);
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
	uint32_t __cdecl ak_77C4D0(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x25B4B54, 0);                  // actor state block
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
	uint32_t __cdecl ak_77C5E0(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				ak_77C660(a1, a2);
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
			ak_77C660(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			ak_77C660(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x828D30 (copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl ak_77C660(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return ak_77C690(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x828D60 (copy of db_84A0E0 0x84A0E0: copy of cl_8770F0 0x8770F0: copy of d_8BBC10: 0x8BBC10 (sub_8BBC10)): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 0x14 records
	uint32_t __cdecl ak_77C690(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x25B4634);
		int32_t idx = MEM<int16_t>(0x25AECD0);
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
			MEM<uint16_t>(0x25AECD0) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x25AECD0) = 0;
		return slot;
	}

	// 0x84A180 (copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl ak_77C730(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x77D0E0, a1);
		else if (mode == 4)
			callp(0x77D230, a1);
		ak_77C7B0(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x7D9D50 (copy of wb_87C2E0: 0x87C2E0 (copy of Curaga cg_881820: 0x881820 (sub_881820; = Confuse c_8631C0, copy of a_737F50 0x737F50; engine; MiniMog sub_737F50))): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl ak_77C7B0(uint32_t a1)
	{
		uint32_t sys = U32(0x25B4B54, 0);
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
				ak_77C830(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			ak_77C830(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x7D9DD0 (copy of wb_87C360: 0x87C360 (copy of Curaga cg_8818A0: 0x8818A0 (sub_8818A0; = Confuse c_863240, copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280))): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl ak_77C830(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x25B4B24, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x25B4B24, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = ak_77D010(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x25B4B54, 0);
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
		ak_77DD20(node, desc);
		U32(0x25B4B24, 0) = U32(0x25B4B24, 0) + 0x50;
		return 0; // void
	}

	// 0x7DA5B0 (copy of c_863A20: 0x863A20 (sub_863A20)): the engine's 0x7387B0 with the module's actor pool: allocates an actor
	// record of 0x2A0 bytes from the pool [0x26A9BB4] starting at the cursor 0x26A9BA8 (wraps at 49:
	// record 49 of the 50 is never used), cleared, owner a1, definition index a2
	uint32_t __cdecl ak_77D010(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x25B4B48);
		int32_t i = MEM<int16_t>(0x25B4640);
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
			if (i >= 0x31)
				i = 0;
			tries++;
			if (tries >= 0x32)
				break;
		}
		i++;
		if (i >= 0x31)
			MEM<uint16_t>(0x25B4640) = 0;
		else
			MEM<uint16_t>(0x25B4640) = (uint16_t)i;
		return slot;
	}

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl ak_77D230(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x25B4B54, 0);            // ecx
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
				uint32_t g = U32(0x25B4B54, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x25B4B54, 0);
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
	uint32_t __cdecl ak_77DD20(uint32_t a1, uint32_t a2)
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
			ak_77C690(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			ak_77C690(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			ak_77C690(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			ak_77C690(obj, id, variant);
		}
		return 0; // void
	}

	// 0x82A480 (copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via ak_77C690(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl ak_77DDB0(void)
	{
		uint32_t dir = MEM<uint32_t>(0x25B4B54);  // eax (re-read only after the calls)
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
					ak_77C690(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x25B4B54);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				ak_77C690(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x25B4B54);
			}
		}
		return 0; // void
	}

	// 0x8CC1B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl ak_77DE40(uint32_t a1)
	{
		if (ak_779C10(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x77DE70 (sub_77DE70): Angelo state 6 - when the animation ends (0x8DD220): animation 4, 0x200 higher; the carry: from his
	// position to his position (+0x140.. = +0x148..; +0x14A = -0x1800), factor +0x168 = 0 on the eased table 0x1561744 (+0x174 =
	// 0); the target's offset from him (+0x160..) turned by -(his facing +0x3E); pitch +0x3C = 0xC00; the halo 0x77DFC0 (queue
	// 0x25AECD8, node 0x98, +0x8C = his model node + 0x30), the second dust play 0x7764B0 (+0x8A = 1); countdown +0x134 = 8; the
	// first carry step (0x77E260 eased), next state
	uint32_t __cdecl ak_77DE70(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t mdl = node + 0x30;
		const uint32_t t = gx::EntityChainLowest(ENT(U8(node, 0x2D)));
		if (x::au_re_Battle_ReadAnimation_8(node) != 1)
			return 0; // void
		x::au_re_Battle_ReadAnimation_7(node, 4);
		U16(node, 0x4E) = (uint16_t)(U16(node, 0x4E) + 0xFE00);
		const uint32_t pxy = U32(node, 0x4C), pz = U32(node, 0x50);
		const uint16_t py = U16(node, 0x4E);
		U32(node, 0x148) = pxy;
		U32(node, 0x140) = pxy;
		U32(node, 0x14C) = pz;
		U16(node, 0x168) = 0;
		U16(node, 0x174) = 0;
		const uint16_t ox = (uint16_t)(U16(t, 0x1C) - U16(node, 0x4C));
		U32(node, 0x144) = pz;
		const uint16_t oy = (uint16_t)(U16(t, 0x1E) - py);
		const uint16_t oz = (uint16_t)(U16(t, 0x20) - U16(node, 0x50));
		U16(node, 0x3C) = 0xC00;
		U16(node, 0x160) = ox;
		U16(node, 0x14A) = 0xE800;
		U16(node, 0x162) = oy;
		U16(node, 0x164) = oz;
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(0x1000 - (int32_t)S16(node, 0x3E)));
		x::matrixMultiplyVector(M, node + 0x160, node + 0x160);
		const uint32_t h = x::Effect_AddTaskAndInitFromCtx(0x25AECD8, 0x77DFC0, 0x98, node);
		U32(h, 0x8C) = mdl;
		const uint32_t p = ak_776460(node, 0x7764B0, 0x16FD248, 0xB0, 0, 0);
		U32(p, 0x240) = mdl;
		U16(p, 0x8A) = 1;
		U16(node, 0x134) = 8;
		ak_77E260(node, 1);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x77DFC0 (sub_77DFC0): HALO task (node 0x98) - states {0x77E180 set-up, 0x77E1E0 fade-in (+0x50 down to 0), 0x77E210 (frame
	// 0xA), 0x77E220 (fade-out +0x50 up to 0x1000: finished), ret}; spin +0x46 += 0x100; height +0x34 = - the y of Angelo's bone
	// point 0x1D; drawn by 0x77E060; frame +0x24; released once finished with no child alive
	uint32_t __cdecl ak_77DFC0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x77E180;
		states[1] = 0x77E1E0;
		states[2] = 0x77E210;
		states[3] = 0x77E220;
		states[4] = 0x77E250; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		alignas(4) uint8_t pt[8] = {};
		U16(node, 0x46) = (uint16_t)((U16(node, 0x46) + 0x100) & 0xFFF);
		x::GetEffectSpawnPosition(U32(node, 0x8C), 0x1D, 0, P(pt));
		S32(node, 0x34) = -(int32_t)S16(P(pt), 2);
		ak_77E060(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x77E060 (sub_77E060): a model piece (the halo / the debris) drawn unless hidden (+0x26 bit 2): rotation y +0x46, x +0x44,
	// z +0x48 (0x8DD770 / 0x8DD8A0 / 0x8DD7E0 / 0x8DD960) at the position +0x1C..+0x20, scaled by +0x30.., composed with the camera
	// 0x1D97778, into the GTE; a 0x68-byte render header (model +0x4C, +0x40, +0x50, +0x52, flags 0xF0, unit scales 0x100) drawn
	// by the module's prim-model renderer 0x776920 into the effect OT from the frame arena 0x1D8E054
	uint32_t __cdecl ak_77E060(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_draw(HK_MODEL, node);)
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
		MEM<uint32_t>(0x1D8E054) = ak_776920(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
		x::Field_Free(0x68);
		return 0; // void
	}

	// 0x77E180 (sub_77E180): halo state 0 - model 0x1557FBC, scale 0x800 / 0x1000 / 0x800, depth +0x52 = -0x200, fade +0x50 =
	// 0x1000, at Angelo's point 0xF1 on the ground, next state
	uint32_t __cdecl ak_77E180(uint32_t a1)
	{
		S32(a1, 0x30) = 0x800;
		S32(a1, 0x38) = 0x800;
		S32(a1, 0x34) = 0x1000;
		U16(a1, 0x50) = 0x1000;
		U32(a1, 0x4C) = 0x1557FBC;
		U16(a1, 0x52) = 0xFE00;
		x::GetEffectSpawnPosition(U32(a1, 0x8C), 0xF1, 0, a1 + 0x1C);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		U16(a1, 0x1E) = 0;
		return 0; // void
	}

	// 0x77E1E0 (sub_77E1E0): halo state 1 - fade +0x50 -= 0x200; at 0 (or below) 0, next state
	uint32_t __cdecl ak_77E1E0(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0xFE00);
		if (S16(a1, 0x50) <= 0)
		{
			U16(a1, 0x50) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77E210 (sub_77E210): halo state 2 - next state from frame 0xA
	uint32_t __cdecl ak_77E210(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0xA)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x745610 (module 095): +0x50 += 0x200; at >= 0x1000: finished (+0x26 |= 1), clamp 0x1000,
	// next state
	uint32_t __cdecl ak_77E220(uint32_t a1)
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

	// 0x77E260 (sub_77E260): Angelo's carry - factor +0x168: a2 == 0: speed +0x16A += +0x16C, factor += speed; else the eased
	// table 0x1561744 at +0x174 (which advances until the table reaches 0x1000); position from +0x140.. towards +0x148.. by the
	// factor (at 0x1000 the end), y += the height +0x16E; the target (the lowest entity of the +0x2D chain) carried: its pitch
	// (entity +0xC) = 0xFC00 - his pitch +0x3C, its position (+0x1C.. s16, +0x54.. s32) = his + the offset +0x160.. turned by his
	// yaw +0x3E and pitch +0x3C + 0x400, its matrix (+0x40) from its angles (ComposeZYXRotationMatrix); the entities chained to it
	// (+0x8C) with flag bit 1 get its position, angles and matrix
	uint32_t __cdecl ak_77E260(uint32_t a1, uint32_t a2)
	{
		const uint32_t n = a1;
		const uint32_t t = gx::EntityChainLowest(ENT(U8(n, 0x2D)));
		if ((int16_t)a2 == 0)
		{
			U16(n, 0x16A) = (uint16_t)(U16(n, 0x16A) + U16(n, 0x16C));
			U16(n, 0x168) = (uint16_t)(U16(n, 0x168) + U16(n, 0x16A));
		}
		else
		{
			const int16_t i = S16(n, 0x174);
			const uint16_t f = MEM<uint16_t>(0x1561744 + (uint32_t)(int32_t)i * 2);
			U16(n, 0x168) = f;
			if (f != 0x1000)
				U16(n, 0x174) = (uint16_t)(i + 1);
		}
		const int16_t f = S16(n, 0x168);
		if (f >= 0x1000)
		{
			U32(n, 0x4C) = U32(n, 0x148);
			U16(n, 0x168) = 0x1000;
			U32(n, 0x50) = U32(n, 0x14C);
		}
		else
		{
			const int32_t k = f;
			for (uint32_t o = 0; o < 6; o += 2)
			{
				const int16_t a = S16(n, 0x140 + o);
				U16(n, 0x4C + o) = (uint16_t)(((int32_t)S16(n, 0x148 + o) - a) * k / 4096 + a);
			}
		}
		U16(n, 0x4E) = (uint16_t)(U16(n, 0x4E) + U16(n, 0x16E));
		U16(t, 0xC) = (uint16_t)((0xFC00 - U16(n, 0x3C)) & 0xFFF);
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(n, 0x3E));
		x::sub_8DD7E0(M, (uint32_t)((int32_t)S16(n, 0x3C) + 0x400));
		alignas(4) uint8_t v[8];
		U32(P(v), 0) = U32(n, 0x160);
		U32(P(v), 4) = U32(n, 0x164);
		x::matrixMultiplyVector(M, n + 0x160, P(v));
		const int32_t px = (int32_t)S16(n, 0x4C) + S16(P(v), 0);
		S32(t, 0x54) = px;
		U16(t, 0x1C) = (uint16_t)px;
		const int32_t py = (int32_t)S16(n, 0x4E) + S16(P(v), 2);
		S32(t, 0x58) = py;
		U16(t, 0x1E) = (uint16_t)py;
		const int32_t pz = (int32_t)S16(n, 0x50) + S16(P(v), 4);
		S32(t, 0x5C) = pz;
		U16(t, 0x20) = (uint16_t)pz;
		x::ComposeZYXRotationMatrix(t + 0xC, t + 0x40);
		uint32_t e = t;
		do
		{
			if ((U8(e, 0) & 2) != 0)
			{
				U32(e, 0x1C) = U32(t, 0x1C);
				U32(e, 0x20) = U32(t, 0x20);
				U32(e, 0xC) = U32(t, 0xC);
				U32(e, 0x10) = U32(t, 0x10);
				memmove((void *)(e + 0x40), (const void *)(t + 0x40), 8 * 4); // rep movsd
			}
			e = U32(e, 0x8C);
		} while (e != t && e != 0);
		return 0; // void
	}

	// 0x77E490 (sub_77E490): Angelo state 7 - the eased carry step (0x77E260, 1), animation step (0x8DD1C0); countdown +0x134; at
	// 0 next state
	uint32_t __cdecl ak_77E490(uint32_t a1)
	{
		const uint32_t node = a1;
		ak_77E260(node, 1);
		x::sub_8DD1C0(node);
		U16(node, 0x134) = (uint16_t)(U16(node, 0x134) - 1);
		if (S16(node, 0x134) <= 0)
		{
			U16(node, 0x134) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77E4D0 (sub_77E4D0): Angelo state 8 - the spin: pitch speed +0x176 -= 0x80 (down to -0x300), pitch +0x3C += it (each
	// wrap below 0 counts +0x134); after 4 turns: pitch 0, animation 5, the slam: from the carry's end back to its start (+0x140..
	// / +0x148.. swapped, +0x14A = 0), the target's offset y +0x164 += 0x100, factor 0 (speed 0, acceleration +0x16C = 0xC0), next
	// state; else the eased carry step; then animation step (0x8DD1C0)
	uint32_t __cdecl ak_77E4D0(uint32_t a1)
	{
		const uint32_t node = a1;
		gx::EntityChainLowest(ENT(U8(node, 0x2D)));
		U16(node, 0x176) = (uint16_t)(U16(node, 0x176) - 0x80);
		if (S16(node, 0x176) <= (int16_t)0xFD00)
			U16(node, 0x176) = 0xFD00;
		U16(node, 0x3C) = (uint16_t)(U16(node, 0x3C) + U16(node, 0x176));
		const uint16_t p = U16(node, 0x3C);
		if ((int16_t)p < 0)
		{
			U16(node, 0x134) = (uint16_t)(U16(node, 0x134) + 1);
			U16(node, 0x3C) = (uint16_t)(p & 0xFFF);
		}
		U16(node, 0x3C) = (uint16_t)(U16(node, 0x3C) & 0xFFF);
		if (S16(node, 0x134) >= 4)
		{
			U16(node, 0x3C) = 0;
			x::au_re_Battle_ReadAnimation_7(node, 5);
			const uint32_t e0 = U32(node, 0x148), s0 = U32(node, 0x140), s1 = U32(node, 0x144);
			U16(node, 0x164) = (uint16_t)(U16(node, 0x164) + 0x100);
			U32(node, 0x140) = e0;
			const uint32_t e1 = U32(node, 0x14C);
			U32(node, 0x148) = s0;
			U32(node, 0x14C) = s1;
			U32(node, 0x144) = e1;
			U16(node, 0x14A) = 0;
			U16(node, 0x168) = 0;
			U16(node, 0x16A) = 0;
			U16(node, 0x16C) = 0xC0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		else
			ak_77E260(node, 1);
		x::sub_8DD1C0(node);
		a_73A6D0(); // 0x77E5F0 (nullsub)
		return 0; // void
	}

	// 0x77E600 (sub_77E600): Angelo state 9 - animation step (0x8DD1C0), the accelerating slam (0x77E260, 0); when it lands
	// (factor +0x168 = 0x1000): animation 6, the impact play 0x77E6C0 (layout [loader block +0x128] / 0x1AC, mode 1, +0x240 = his
	// model node + 0x30), the debris spawner 0x77E800 (queue 0x25AECD8, node 0x98) at his position on the ground, the sound
	// 0x1557D30, next state
	uint32_t __cdecl ak_77E600(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t mdl = node + 0x30;
		gx::EntityChainLowest(ENT(U8(node, 0x2D)));
		x::sub_8DD1C0(node);
		ak_77E260(node, 0);
		if (U16(node, 0x168) != 0x1000)
			return 0; // void
		x::au_re_Battle_ReadAnimation_7(node, 6);
		const uint32_t p = ak_776460(node, 0x77E6C0, U32(MEM<uint32_t>(0x25B4448), 0x128), 0x1AC, 1, 0);
		U32(p, 0x240) = mdl;
		const uint32_t d = x::Effect_AddTaskAndInitFromCtx(0x25AECD8, 0x77E800, 0x98, node);
		U32(d, 0x1C) = U32(mdl, 0x1C);
		U32(d, 0x20) = U32(mdl, 0x20);
		U16(d, 0x1E) = 0;
		x::BdPlaySE(0x1557D30, 0, 0x80);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x77E720 (sub_77E720): impact play state 0 (task 0x77E6C0 = a_73A380) - decodes the prim layout (+0x74, count +0x78) into
	// +0x94, placed at Angelo's point 0xF1 (model +0x240) on the ground, scale by the target's size (0x77E780), plays the first
	// frame (0x776580 = a_73C280), next state
	uint32_t __cdecl ak_77E720(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t mdl = U32(node, 0x240);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(mdl, 0xF1, 0, node + 0x1C);
		U16(node, 0x1E) = 0;
		const int32_t s = (int16_t)ak_77E780(node);
		S32(node, 0x58) = s;
		S32(node, 0x54) = s;
		S32(node, 0x50) = s;
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_draw(HK_PLAY, node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x77E780 (sub_77E780): the impact's scale from the target's size (entity +0x26 of the lowest entity of the +0x2D chain):
	// below 0x800 0x1000, else 0x1000 + (size - 0x800) / 2 up to 0x2000
	uint32_t __cdecl ak_77E780(uint32_t a1)
	{
		const uint32_t t = gx::EntityChainLowest(ENT(U8(a1, 0x2D)));
		const int16_t size = S16(t, 0x26);
		if (size < 0x800)
			return 0x1000;
		int32_t v = ((int32_t)size - 0x800) / 2 + 0x1000;
		if ((int16_t)v > 0x2000)
			v = 0x2000;
		return (uint32_t)v;
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl ak_77E7D0(uint32_t a1)
	{
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(held_note_draw(HK_PLAY, a1);)
		const uint32_t r = a_73C280(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x720790 (module 099 sub_720790): TASK - state machine {7207E0, nop}, task tail (no draw).
	uint32_t __cdecl ak_77E800(uint32_t a1)
	{
		const uint32_t states[2] = { 0x77E850, 0x77ED40 };
		callp(states[S8(a1, 0x29)], a1);
		return task_end(a1);
	}

	// 0x77E850 (sub_77E850): debris spawner state 0 (task 0x77E800) - on its frames 0..5: table 0x1560538[frame] large pieces
	// 0x77EAB0 and table 0x1560544[frame] small pieces 0x77EC00 (queue 0x25AECD8, node 0x98) at its position + a random distance
	// 0x100..0x4FF in a random direction (+0x46 = that yaw), velocity +0x7C / +0x80 = 0x300..0x6FF outwards, +0x7E = -(0xE00 / 0x1000
	// + 0..0xFFF) up; from frame 6 finished, next state
	uint32_t __cdecl ak_77E850(uint32_t a1)
	{
		const uint32_t node = a1;
		const int16_t f = S16(node, 0x24);
		if (f >= 6)
		{
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 1);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			return 0; // void
		}
		for (int kind = 0; kind < 2; kind++)
		{
			const int16_t fr = S16(node, 0x24);
			int32_t cnt = MEM<int16_t>((kind == 0 ? 0x1560538 : 0x1560544) + (uint32_t)(int32_t)fr * 2);
			if (cnt <= 0)
				continue;
			do
			{
				const int32_t ang = (int32_t)(int16_t)(x::CrtRand() & 0xFFF);
				const int32_t dist = (int32_t)(int16_t)((x::CrtRand() & 0x3FF) + 0x100);
				const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25AECD8, kind == 0 ? 0x77EAB0 : 0x77EC00, 0x98, node);
				U32(t, 0x1C) = U32(node, 0x1C);
				U32(t, 0x20) = U32(node, 0x20);
				U16(t, 0x1C) = (uint16_t)(U16(t, 0x1C) + (uint16_t)((int32_t)x::computeSin((uint32_t)ang) * dist / 4096));
				U16(t, 0x20) = (uint16_t)(U16(t, 0x20) + (uint16_t)((int32_t)x::computeCosine((uint32_t)ang) * dist / 4096));
				const int32_t speed = (int32_t)(int16_t)((x::CrtRand() & 0x3FF) + 0x300);
				const uint32_t up = x::CrtRand() & 0xFFF;
				U16(t, 0x7E) = (uint16_t)((kind == 0 ? 0xFFFFF200u : 0xFFFFF000u) - up);
				U16(t, 0x7C) = (uint16_t)((int32_t)x::computeSin((uint32_t)ang) * speed / 4096);
				U16(t, 0x46) = (uint16_t)ang;
				U16(t, 0x80) = (uint16_t)((int32_t)x::computeCosine((uint32_t)ang) * speed / 4096);
			} while (--cnt != 0);
		}
		return 0; // void
	}

	// 0x77EAB0 / 0x77EC00 (sub_77EAB0 / sub_77EC00): DEBRIS piece tasks (node 0x98) - states {0x77EB70 / 0x77ECC0 set-up, 0x77EBD0
	// / 0x77ED10 (finished at frame 0x19 / 0x10), ret}; gravity +0x7E += 0x200 / 0x400, tumble +0x44 += +0x96, position +=
	// velocity / 16; drawn by 0x77E060; frame +0x24; released once finished with no child alive
	static inline uint32_t ak_debris(uint32_t node, const uint32_t (&states)[3], uint16_t gravity)
	{
		callp(states[S8(node, 0x29)], node);
		const int32_t vx = S16(node, 0x7C);
		U16(node, 0x7E) = (uint16_t)(U16(node, 0x7E) + gravity);
		U16(node, 0x44) = (uint16_t)((U16(node, 0x96) + U16(node, 0x44)) & 0xFFF);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + (uint16_t)(vx / 16));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + (uint16_t)((int32_t)S16(node, 0x7E) / 16));
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + (uint16_t)((int32_t)S16(node, 0x80) / 16));
		ak_77E060(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	uint32_t __cdecl ak_77EAB0(uint32_t a1)
	{
		static const uint32_t states[3] = { 0x77EB70, 0x77EBD0, 0x77EBF0 /* nullsub (ret) */ };
		return ak_debris(a1, states, 0x200);
	}

	// 0x77EB70 (sub_77EB70): large debris state 0 - model 0x155AA5C or 0x155ADF4 (random bit 0), scale 0x100 + (0..0xFFE)^3,
	// tumble speed +0x96 = -0x40 - (0..0x3F), next state
	uint32_t __cdecl ak_77EB70(uint32_t a1)
	{
		const uint32_t r1 = x::CrtRand();
		U32(a1, 0x4C) = (r1 & 1) ? 0x155AA5C : 0x155ADF4;
		const int32_t s = (int32_t)x::CrtRand() % 0xFFF + 0x100;
		S32(a1, 0x38) = s;
		S32(a1, 0x30) = s;
		S32(a1, 0x34) = s;
		const uint32_t r3 = x::CrtRand();
		U16(a1, 0x96) = (uint16_t)(0xFFFFFFC0u - (r3 & 0x3F));
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x77EBD0 (sub_77EBD0): large debris state 1 - from frame 0x19 finished, next state
	uint32_t __cdecl ak_77EBD0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x19)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	uint32_t __cdecl ak_77EC00(uint32_t a1)
	{
		static const uint32_t states[3] = { 0x77ECC0, 0x77ED10, 0x77ED30 /* nullsub (ret) */ };
		return ak_debris(a1, states, 0x400);
	}

	// 0x77ECC0 (sub_77ECC0): small debris state 0 - model 0x155B12C, scale 0x800 + (0..0x7FE)^3, tumble speed +0x96 = -0x40 -
	// (0..0x3F), next state
	uint32_t __cdecl ak_77ECC0(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x155B12C;
		const int32_t s = (int32_t)x::CrtRand() % 0x7FF + 0x800;
		S32(a1, 0x38) = s;
		S32(a1, 0x30) = s;
		S32(a1, 0x34) = s;
		const uint32_t r2 = x::CrtRand();
		U16(a1, 0x96) = (uint16_t)(0xFFFFFFC0u - (r2 & 0x3F));
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x77ED10 (sub_77ED10): small debris state 1 - from frame 0x10 finished, next state
	uint32_t __cdecl ak_77ED10(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x10)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77ED50 (sub_77ED50): Angelo state 10 - when the animation ends (0x8DD220): animation 0xB, the jump back: from his position
	// (+0x140..) to the caster's (+0x2C: entity +0x1C / +0x20, +0x148..), factor 0 (speed +0x16A = 0x125), height 0 (rising at
	// -0x200, gravity 0x49), next state
	uint32_t __cdecl ak_77ED50(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t c = ENT(U8(node, 0x2C));
		if (x::au_re_Battle_ReadAnimation_8(node) != 1)
			return 0; // void
		x::au_re_Battle_ReadAnimation_7(node, 0xB);
		U32(node, 0x140) = U32(node, 0x4C);
		U32(node, 0x144) = U32(node, 0x50);
		U16(node, 0x168) = 0;
		U16(node, 0x16C) = 0;
		U16(node, 0x16E) = 0;
		U32(node, 0x148) = U32(c, 0x1C);
		U32(node, 0x14C) = U32(c, 0x20);
		U16(node, 0x16A) = 0x125;
		U16(node, 0x170) = 0xFE00;
		U16(node, 0x172) = 0x49;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x77EDF0 (sub_77EDF0): Angelo state 11 - height step, the jump (0x7762D0); when the animation ends (0x8DD220) animation 7,
	// next state
	uint32_t __cdecl ak_77EDF0(uint32_t a1)
	{
		const uint32_t node = a1;
		ak_height_step(node);
		ak_7762D0(node);
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 7);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77EE50 (sub_77EE50): Angelo state 12 - height step, the jump (0x7762D0), animation step (0x8DD1C0); from animation frame
	// +0x136 = 0xA animation 0xC, next state
	uint32_t __cdecl ak_77EE50(uint32_t a1)
	{
		const uint32_t node = a1;
		ak_height_step(node);
		ak_7762D0(node);
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) >= 0xA)
		{
			x::au_re_Battle_ReadAnimation_7(node, 0xC);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77EEC0 (sub_77EEC0): Angelo state 13 - height step, the jump (0x7762D0); when the animation ends (0x8DD220): the target's
	// rotation / position put back (entity +0xC.. / +0x1C.. from +0x150.. / +0x158..), animation 8, Rinoa's chain
	// transformation 2, next state
	uint32_t __cdecl ak_77EEC0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t c = ENT(U8(node, 0x2C));
		const uint32_t t = gx::EntityChainLowest(ENT(U8(node, 0x2D)));
		ak_height_step(node);
		ak_7762D0(node);
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			U32(t, 0xC) = U32(node, 0x150);
			U32(t, 0x1C) = U32(node, 0x158);
			U32(t, 0x10) = U32(node, 0x154);
			U32(t, 0x20) = U32(node, 0x15C);
			x::au_re_Battle_ReadAnimation_7(node, 8);
			gx::QueueChainTransformation(c, 2);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77EF90 (sub_77EF90): Angelo state 14 - when the animation ends (0x8DD220): animation 9, walks off (0x77EFD0, 0x100), next
	// state
	uint32_t __cdecl ak_77EF90(uint32_t a1)
	{
		const uint32_t node = a1;
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 9);
			ak_77EFD0(node, 0x100);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77EFD0 (sub_77EFD0): Angelo's velocity +0x124.. = (0, 0, a2) turned by his facing +0x3E
	uint32_t __cdecl ak_77EFD0(uint32_t a1, uint32_t a2)
	{
		alignas(4) uint8_t m[0x20];
		const uint32_t M = P(m);
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(a1, 0x3E));
		U16(a1, 0x124) = 0;
		U16(a1, 0x126) = 0;
		U16(a1, 0x128) = (uint16_t)a2;
		return x::matrixMultiplyVector(M, a1 + 0x124, a1 + 0x124);
	}

	// 0x77F030 (sub_77F030): Angelo state 15 - turns (0x77F070), walks (0x77EFD0, 0x100), animation step (0x8DD1C0); from animation
	// frame +0x136 = 0x12 hidden + finished, next state
	uint32_t __cdecl ak_77F030(uint32_t a1)
	{
		const uint32_t node = a1;
		ak_77F070(node);
		ak_77EFD0(node, 0x100);
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) >= 0x12)
		{
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 5);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x77F070 (sub_77F070): Angelo's turn towards facing 0 by 0x10 a tick (the short way; at the crossing 0)
	uint32_t __cdecl ak_77F070(uint32_t a1)
	{
		U16(a1, 0x3E) = (uint16_t)(U16(a1, 0x3E) & 0xFFF);
		const int16_t f = S16(a1, 0x3E);
		if (f > 0x800)
		{
			const uint16_t v = (uint16_t)((f + 0x10) & 0xFFF);
			U16(a1, 0x3E) = v;
			if ((int16_t)v < 0x800)
				U16(a1, 0x3E) = 0;
		}
		else if (f < 0x800)
		{
			const uint16_t v = (uint16_t)((f - 0x10) & 0xFFF);
			U16(a1, 0x3E) = v;
			if ((int16_t)v > 0x800)
				U16(a1, 0x3E) = 0;
		}
		return a1;
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl ak_77F0D0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1557D28);
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

	// ---- generated (desc87.py / cls87c.txt): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x773BA0, (void *)a_73A0D0, "087 MAG_087_sub_773BA0" },
		{ 0x773BB0, (void *)a_73A0D0, "087 MAG_087_sub_773BB0" },
		{ 0x773C40, (void *)a_73A170, "087 MAG_087_sub_773C40" },
		{ 0x773C70, (void *)a_73A1A0, "087 MAG_087_sub_773C70" },
		{ 0x773E30, (void *)a_73A380, "087 MAG_087_sub_773E30" },
		{ 0x773E90, (void *)a_73A3E0, "087 sub_773E90" },
		{ 0x774030, (void *)a_73A580, "087 sub_774030" },
		{ 0x774060, (void *)a_73A5B0, "087 sub_774060" },
		{ 0x774090, (void *)a_73A5E0, "087 sub_774090" },
		{ 0x7740A0, (void *)a_73A5E0, "087 sub_7740A0" },
		{ 0x7740B0, (void *)a_73A5E0, "087 sub_7740B0" },
		{ 0x7740C0, (void *)a_73A5E0, "087 sub_7740C0" },
		{ 0x7740D0, (void *)a_73A620, "087 sub_7740D0" },
		{ 0x774110, (void *)a_73A660, "087 sub_774110" },
		{ 0x774180, (void *)a_73A6D0, "087 nullsub_1282" },
		{ 0x774370, (void *)a_73B640, "087 sub_774370" },
		{ 0x774490, (void *)a_73AB00, "087 sub_774490" },
		{ 0x7744E0, (void *)a_73AAE0, "087 sub_7744E0" },
		{ 0x774500, (void *)a_73AE10, "087 sub_774500" },
		{ 0x774850, (void *)a_73B160, "087 sub_774850" },
		{ 0x7749C0, (void *)a_73AB20, "087 sub_7749C0" },
		{ 0x774EF0, (void *)a_73B2D0, "087 sub_774EF0" },
		{ 0x774F30, (void *)a_73B750, "087 sub_774F30" },
		{ 0x774F50, (void *)a_73A6D0, "087 nullsub_1283" },
		{ 0x774F60, (void *)a_73A6D0, "087 nullsub_1284" },
		{ 0x774F90, (void *)a_73B7E0, "087 sub_774F90" },
		{ 0x774FB0, (void *)a_73A930, "087 sub_774FB0" },
		{ 0x774FD0, (void *)a_73A950, "087 sub_774FD0" },
		{ 0x775050, (void *)a_73BB60, "087 sub_775050" },
		{ 0x7750A0, (void *)a_73B8A0, "087 sub_7750A0" },
		{ 0x7750C0, (void *)a_73B8C0, "087 sub_7750C0" },
		{ 0x775100, (void *)a_733B70, "087 sub_775100" },
		{ 0x775140, (void *)a_73A6D0, "087 nullsub_1285" },
		{ 0x775150, (void *)a_73A6D0, "087 nullsub_1286" },
		{ 0x775330, (void *)a_746C10, "087 sub_775330" },
		{ 0x7758A0, (void *)a_7435E0, "087 InitEffectSequenceFromData_c7" },
		{ 0x7759E0, (void *)a_743720, "087 sub_7759E0" },
		{ 0x775F00, (void *)a_743C20, "087 sub_775F00" },
		{ 0x775F30, (void *)a_73A6D0, "087 nullsub_1287" },
		{ 0x7764B0, (void *)a_73A380, "087 sub_7764B0" },
		{ 0x776580, (void *)a_73C280, "087 sub_776580" },
		{ 0x777900, (void *)a_73D770, "087 sub_777900" },
		{ 0x777B50, (void *)a_73D9C0, "087 sub_777B50" },
		{ 0x7789A0, (void *)a_73A6D0, "087 nullsub_1288" },
		{ 0x779000, (void *)a_73C100, "087 sub_779000" },
		{ 0x779050, (void *)a_73A380, "087 sub_779050" },
		{ 0x779130, (void *)a_73F990, "087 sub_779130" },
		{ 0x7793C0, (void *)a_73FC20, "087 sub_7793C0" },
		{ 0x779630, (void *)a_73FE90, "087 sub_779630" },
		{ 0x779750, (void *)a_73FFB0, "087 sub_779750" },
		{ 0x7799B0, (void *)a_740210, "087 sub_7799B0" },
		{ 0x779CA0, (void *)a_735440, "087 sub_779CA0" },
		{ 0x779D00, (void *)a_7354A0, "087 sub_779D00" },
		{ 0x779E40, (void *)a_7355E0, "087 sub_779E40" },
		{ 0x77A410, (void *)a_735BB0, "087 sub_77A410" },
		{ 0x77A650, (void *)a_735DF0, "087 sub_77A650" },
		{ 0x77B530, (void *)a_740910, "087 sub_77B530" },
		{ 0x77BB90, (void *)a_740F70, "087 sub_77BB90" },
		{ 0x77BBE0, (void *)a_740FC0, "087 sub_77BBE0" },
		{ 0x77BC70, (void *)a_741050, "087 sub_77BC70" },
		{ 0x77BD40, (void *)a_741120, "087 sub_77BD40" },
		{ 0x77C780, (void *)a_741B60, "087 sub_77C780" },
		{ 0x77CEB0, (void *)a_742290, "087 MAG_087_RandRange" },
		{ 0x77CEE0, (void *)a_7422C0, "087 sub_77CEE0" },
		{ 0x77CF20, (void *)a_742300, "087 sub_77CF20" },
		{ 0x77CF70, (void *)a_742350, "087 sub_77CF70" },
		{ 0x77CFC0, (void *)a_7423A0, "087 MAG_087_RandAngleJitter" },
		{ 0x77D0D0, (void *)a_7424B0, "087 sub_77D0D0" },
		{ 0x77D0E0, (void *)a_7424C0, "087 sub_77D0E0" },
		{ 0x77D8C0, (void *)a_742CA0, "087 sub_77D8C0" },
		{ 0x77D8F0, (void *)a_742CD0, "087 sub_77D8F0" },
		{ 0x77D920, (void *)a_742D00, "087 sub_77D920" },
		{ 0x77DAD0, (void *)a_742EB0, "087 sub_77DAD0" },
		{ 0x77DC10, (void *)a_742FF0, "087 sub_77DC10" },
		{ 0x77DE60, (void *)a_73A6D0, "087 nullsub_1297" },
		{ 0x77E250, (void *)a_73A6D0, "087 nullsub_1289" },
		{ 0x77E5F0, (void *)a_73A6D0, "087 nullsub_1290" },
		{ 0x77E6C0, (void *)a_73A380, "087 sub_77E6C0" },
		{ 0x77E7F0, (void *)a_73A6D0, "087 nullsub_1291" },
		{ 0x77EBF0, (void *)a_73A6D0, "087 nullsub_1292" },
		{ 0x77ED30, (void *)a_73A6D0, "087 nullsub_1293" },
		{ 0x77ED40, (void *)a_73A6D0, "087 nullsub_1294" },
		{ 0x77F0C0, (void *)a_73A6D0, "087 nullsub_1295" },
		{ 0x77F110, (void *)a_7474B0, "087 MAG_087_sub_77F110" },
		{ 0x77F130, (void *)a_7474D0, "087 MAG_087_sub_77F130" },
		{ 0x77F150, (void *)a_73A6D0, "087 nullsub_1296" },
		{ 0x77F160, (void *)a_747500, "087 MAG_087_sub_77F160" },
		{ 0x77F1A0, (void *)a_73A0D0, "087 MAG_087_sub_77F1A0" },
		{ 0x77F1B0, (void *)a_747550, "087 MAG_087_sub_77F1B0" },
		{ 0x77F1C0, (void *)a_747560, "087 MAG_087_sub_77F1C0" },
		{ 0x77F1F0, (void *)a_747590, "087 MAG_087_sub_77F1F0" },
		{ 0x77F200, (void *)a_7475A0, "087 MAG_087_sub_77F200" },
		{ 0x77F220, (void *)a_73A6D0, "087 nullsub_1281" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x773A10, (void *)ak_773A10, "087 ak_773A10" },
		{ 0x773BC0, (void *)ak_773BC0, "087 ak_773BC0" },
		{ 0x773C00, (void *)ak_773C00, "087 ak_773C00" },
		{ 0x773CF0, (void *)ak_773CF0, "087 ak_773CF0" },
		{ 0x773D40, (void *)ak_773D40, "087 ak_773D40" },
		{ 0x774190, (void *)ak_774190, "087 ak_774190" },
		{ 0x774240, (void *)ak_774240, "087 ak_774240" },
		{ 0x7742E0, (void *)ak_7742E0, "087 ak_7742E0" },
		{ 0x774330, (void *)ak_774330, "087 ak_774330" },
		{ 0x7743A0, (void *)ak_7743A0, "087 ak_7743A0" },
		{ 0x774450, (void *)ak_774450, "087 ak_774450" },
		{ 0x7744B0, (void *)ak_7744B0, "087 ak_7744B0" },
		{ 0x774CB0, (void *)ak_774CB0, "087 ak_774CB0" },
		{ 0x774CE0, (void *)ak_774CE0, "087 ak_774CE0" },
		{ 0x774D40, (void *)ak_774D40, "087 ak_774D40" },
		{ 0x774D90, (void *)ak_774D90, "087 ak_774D90" },
		{ 0x774DE0, (void *)ak_774DE0, "087 ak_774DE0" },
		{ 0x774E20, (void *)ak_774E20, "087 ak_774E20" },
		{ 0x774E90, (void *)ak_774E90, "087 ak_774E90" },
		{ 0x774EC0, (void *)ak_774EC0, "087 ak_774EC0" },
		{ 0x774F10, (void *)ak_774F10, "087 ak_774F10" },
		{ 0x774F70, (void *)ak_774F70, "087 ak_774F70" },
		{ 0x775010, (void *)ak_775010, "087 ak_775010" },
		{ 0x7750E0, (void *)ak_7750E0, "087 ak_7750E0" },
		{ 0x775120, (void *)ak_775120, "087 ak_775120" },
		{ 0x775160, (void *)ak_775160, "087 ak_775160" },
		{ 0x7751A0, (void *)ak_7751A0, "087 ak_7751A0" },
		{ 0x775510, (void *)ak_775510, "087 ak_775510" },
		{ 0x775720, (void *)ak_775720, "087 ak_775720" },
		{ 0x775780, (void *)ak_775780, "087 ak_775780" },
		{ 0x775E50, (void *)ak_775E50, "087 ak_775E50" },
		{ 0x775E70, (void *)ak_775E70, "087 ak_775E70" },
		{ 0x775F40, (void *)ak_775F40, "087 ak_775F40" },
		{ 0x776010, (void *)ak_776010, "087 ak_776010" },
		{ 0x7760E0, (void *)ak_7760E0, "087 ak_7760E0" },
		{ 0x776260, (void *)ak_776260, "087 ak_776260" },
		{ 0x7762D0, (void *)ak_7762D0, "087 ak_7762D0" },
		{ 0x7763B0, (void *)ak_7763B0, "087 ak_7763B0" },
		{ 0x776460, (void *)ak_776460, "087 ak_776460" },
		{ 0x776510, (void *)ak_776510, "087 ak_776510" },
		{ 0x7766B0, (void *)ak_7766B0, "087 ak_7766B0" },
		{ 0x776920, (void *)ak_776920, "087 ak_776920" },
		{ 0x776A50, (void *)ak_776A50, "087 ak_776A50" },
		{ 0x776C70, (void *)ak_776C70, "087 ak_776C70" },
		{ 0x776F00, (void *)ak_776F00, "087 ak_776F00" },
		{ 0x777380, (void *)ak_777380, "087 ak_777380" },
		{ 0x777E30, (void *)ak_777E30, "087 ak_777E30" },
		{ 0x778330, (void *)ak_778330, "087 ak_778330" },
		{ 0x778980, (void *)ak_778980, "087 ak_778980" },
		{ 0x7789B0, (void *)ak_7789B0, "087 ak_7789B0" },
		{ 0x778A20, (void *)ak_778A20, "087 ak_778A20" },
		{ 0x778AB0, (void *)ak_778AB0, "087 ak_778AB0" },
		{ 0x778E90, (void *)ak_778E90, "087 ak_778E90" },
		{ 0x778F90, (void *)ak_778F90, "087 ak_778F90" },
		{ 0x7790B0, (void *)ak_7790B0, "087 ak_7790B0" },
		{ 0x7792E0, (void *)ak_7792E0, "087 ak_7792E0" },
		{ 0x779560, (void *)ak_779560, "087 ak_779560" },
		{ 0x779C10, (void *)ak_779C10, "087 ak_779C10" },
		{ 0x779F80, (void *)ak_779F80, "087 ak_779F80" },
		{ 0x77A190, (void *)ak_77A190, "087 ak_77A190" },
		{ 0x77A8F0, (void *)ak_77A8F0, "087 ak_77A8F0" },
		{ 0x77AB20, (void *)ak_77AB20, "087 ak_77AB20" },
		{ 0x77ADE0, (void *)ak_77ADE0, "087 ak_77ADE0" },
		{ 0x77B040, (void *)ak_77B040, "087 ak_77B040" },
		{ 0x77B320, (void *)ak_77B320, "087 ak_77B320" },
		{ 0x77B3E0, (void *)ak_77B3E0, "087 ak_77B3E0" },
		{ 0x77B460, (void *)ak_77B460, "087 ak_77B460" },
		{ 0x77B4A0, (void *)ak_77B4A0, "087 ak_77B4A0" },
		{ 0x77C4D0, (void *)ak_77C4D0, "087 ak_77C4D0" },
		{ 0x77C5E0, (void *)ak_77C5E0, "087 ak_77C5E0" },
		{ 0x77C660, (void *)ak_77C660, "087 ak_77C660" },
		{ 0x77C690, (void *)ak_77C690, "087 ak_77C690" },
		{ 0x77C730, (void *)ak_77C730, "087 ak_77C730" },
		{ 0x77C7B0, (void *)ak_77C7B0, "087 ak_77C7B0" },
		{ 0x77C830, (void *)ak_77C830, "087 ak_77C830" },
		{ 0x77D010, (void *)ak_77D010, "087 ak_77D010" },
		{ 0x77D230, (void *)ak_77D230, "087 ak_77D230" },
		{ 0x77DD20, (void *)ak_77DD20, "087 ak_77DD20" },
		{ 0x77DDB0, (void *)ak_77DDB0, "087 ak_77DDB0" },
		{ 0x77DE40, (void *)ak_77DE40, "087 ak_77DE40" },
		{ 0x77DE70, (void *)ak_77DE70, "087 ak_77DE70" },
		{ 0x77DFC0, (void *)ak_77DFC0, "087 ak_77DFC0" },
		{ 0x77E060, (void *)ak_77E060, "087 ak_77E060" },
		{ 0x77E180, (void *)ak_77E180, "087 ak_77E180" },
		{ 0x77E1E0, (void *)ak_77E1E0, "087 ak_77E1E0" },
		{ 0x77E210, (void *)ak_77E210, "087 ak_77E210" },
		{ 0x77E220, (void *)ak_77E220, "087 ak_77E220" },
		{ 0x77E260, (void *)ak_77E260, "087 ak_77E260" },
		{ 0x77E490, (void *)ak_77E490, "087 ak_77E490" },
		{ 0x77E4D0, (void *)ak_77E4D0, "087 ak_77E4D0" },
		{ 0x77E600, (void *)ak_77E600, "087 ak_77E600" },
		{ 0x77E720, (void *)ak_77E720, "087 ak_77E720" },
		{ 0x77E780, (void *)ak_77E780, "087 ak_77E780" },
		{ 0x77E7D0, (void *)ak_77E7D0, "087 ak_77E7D0" },
		{ 0x77E800, (void *)ak_77E800, "087 ak_77E800" },
		{ 0x77E850, (void *)ak_77E850, "087 ak_77E850" },
		{ 0x77EAB0, (void *)ak_77EAB0, "087 ak_77EAB0" },
		{ 0x77EB70, (void *)ak_77EB70, "087 ak_77EB70" },
		{ 0x77EBD0, (void *)ak_77EBD0, "087 ak_77EBD0" },
		{ 0x77EC00, (void *)ak_77EC00, "087 ak_77EC00" },
		{ 0x77ECC0, (void *)ak_77ECC0, "087 ak_77ECC0" },
		{ 0x77ED10, (void *)ak_77ED10, "087 ak_77ED10" },
		{ 0x77ED50, (void *)ak_77ED50, "087 ak_77ED50" },
		{ 0x77EDF0, (void *)ak_77EDF0, "087 ak_77EDF0" },
		{ 0x77EE50, (void *)ak_77EE50, "087 ak_77EE50" },
		{ 0x77EEC0, (void *)ak_77EEC0, "087 ak_77EEC0" },
		{ 0x77EF90, (void *)ak_77EF90, "087 ak_77EF90" },
		{ 0x77EFD0, (void *)ak_77EFD0, "087 ak_77EFD0" },
		{ 0x77F030, (void *)ak_77F030, "087 ak_77F030" },
		{ 0x77F070, (void *)ak_77F070, "087 ak_77F070" },
		{ 0x77F0D0, (void *)ak_77F0D0, "087 ak_77F0D0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag087_angelo_strike()
	{
		act::register_module(87);
		for (const act::strike87::ModPort *p = act::strike87::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(87, p->addr, p->port, p->name);
		for (const act::strike87::ModPort *p = act::strike87::PORTS; p->addr; p++)
			act::register_module_port(87, p->addr, p->port, p->name);
		// 30 fps layer: see mag087_angelo_strike_held.inc
		FX_HELD(register_mag087_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag087_angelo_strike_held.inc"
#endif
