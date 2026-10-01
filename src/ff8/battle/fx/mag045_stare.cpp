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

// Effect 45: Stare (enemy attack 84 of kernel.bin, used by Vysage c0m054; MAG_045_*; no other
// kernel.bin entry uses effect 45): a copy of the actor/heal effect library (see act_engine.h)
// built like Tonberry's Chef's Knife (mag090_tonberry.cpp: master + director stepping phases,
// prim-model tasks drawn by the eight primitive-list renderers) without an actor system.
//
// Setup MAG_045_STARE 0x836590 (runs once, not ported; file loader MAG_045_STARE_FL 0x836570 = the
// effect's data file 0x15C3670 (mag044.tim, Junk's texture file) -> 0x26543B0, TIM uploaded by the
// setup, packet arenas at file + 0 / + 0xC000 / + 0x18000) plays the camera animation 0x15C3584 and
// creates the root queue 0x2654588 with the master and five task pools: 0x2655250 (4 x 0x58
// emitters), 0x2654490 (3 x 0x48 director), 0x2654598 (8 x 0x30 stage darkening), 0x2654FA0 (3 x
// 0x2D0 prim-model tasks) and 0x2655240 (10 x 0x40, the engine's screen tints; unused).
//   Master (0x8366E0 = Tonberry's master with the view-axis matrix 0x836840 between the third and
//     the fourth queue) - camera copy 0x2793E58, packet cursor 0x26543B4 by tick parity (the
//     module draws into the frame arena 0x1D8E054 only), bone follow, 11-state table: 0x836960 one
//     emitter per action, then the engine's end states.
//   Emitter (0x836990 = the engine's a_73A1A0): state 0x836A10 sets up the director block
//     [0x15C357C] = 0x26553C8 (0x836A40: the target's position, the facing towards it) and spawns
//     the director; 0x8397E0 applies the damage of one target once the director's cue +0x48 is 1.
//   Director (0x836AC0): phase bookkeeping 0x836B30 (a new phase spawns its tasks: 0x836C80), the
//     cue to phase 1 (a_73B790), the sound 0x15C3580 once phase 1 runs, the damage cue 0x20 ticks
//     into the phase (0x836C20), finish.
//   Phase 1 (0x836C80): two eye stares (0x836D60 -> 0x836DC0 / 0x8394C0: prim layout 0x17C4DA0
//     at the caster's points 0x15 / 0x16 moved 0x40 forward along its facing, following them every
//     tick), the target's mark (0x839590 -> 0x8395F0 / 0x839660: prim layout 0x17C6A14 at the
//     target's anchor 0xF1 from tick 0xF) and the stage darkening (0x839690 = the engine's a_73F110:
//     the stage group words 0x1D98992 + k * 0x2C ramp to 0x600 by 0x200, hold to tick 0x29, back to
//     0 by 0x100).
//   Prim-model tasks (node 0x2D0): the prim-model player (0x836EB0 -> 0x701970, callback 0x836FE0,
//     vertex blend buffer 0x2655428) drawn by the module renderer 0x8372A0 (= Tonberry's eight
//     primitive-list renderers); records with flag 0x1000 are placed along the view axes
//     (matrix 0x2655758 built from the camera by the master every tick, 0x836840).
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x26543B0..0x2655778 (file pointer 0x26543B0, packet cursor 0x26543B4, pools,
// queues, arenas, director block 0x26553C8, blend buffer 0x2655428, view-axis matrix 0x2655758).
//
// Library functions: the ones whose code is identical to an engine port (up to module addresses)
// are served by that port (ENGINE_PORTS); the others are ported below (st_XXXXXX), the library ones
// the text of the matching Tonberry / Doom / Psycho Blast / Boko / Curaga port with this module's
// addresses.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace stare
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_045 = { "stare", 45, 0x836570, 0x839920,
		{ 0x0, 0x0, 0x15C357C, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2655250, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x836990, 0x836A10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x836BA0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8397E0, 0x839820, 0x839840, 0x839860, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x26543B4;   // module packet cursor (tick parity; unused by the draws)
	static const uint32_t FRAME_CURSOR = 0x1D8E054;    // frame arena cursor (the prim draws' packets)
	static const uint32_t DIR_PTR = 0x15C357C;         // director block pointer (-> 0x26553C8, 0x54 bytes)
	static const uint32_t VIEW_MATRIX = 0x2655758;     // view-axis matrix (0x836840)
	static const uint32_t BLEND_BUF = 0x2655428;       // vertex blend buffer of the prim callback
	static const uint32_t PRIM_CB = 0x836FE0;          // the prim-model player's object callback
	static const uint32_t Q_EMITTER = 0x2655250, Q_DIRECTOR = 0x2654490, Q_STAGE = 0x2654598, Q_PRIM = 0x2654FA0, Q_TINT = 0x2655240;
	static const uint32_t ORIG_Master = 0x8366E0;
	static const uint32_t ORIG_Eye = 0x836D60;         // eye stare prim-model task (engine task a_73A380)
	static const uint32_t ORIG_Mark = 0x839590;        // target mark prim-model task (engine task a_73A380)
	static const uint32_t ORIG_Darken = 0x839690;      // stage darkening task (engine task a_73F110)
	static inline uint32_t ENT(uint32_t slot) { return 0x1D972C0 + slot * 0x9C; }

	// engine functions not in act::x
	namespace sx
	{
		static inline uint32_t FixedPointCrossProduct(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56BBF0)(a1, a2, a3); }
		static inline uint32_t NormalizeVectorToFixedPoint(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BC50)(a1, a2); }
		static inline uint32_t MAG_063_sub_7015B0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x7015B0)(a1, a2); }
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
	static inline void next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	// the names of the ports the adapted texts come from
	static inline void t1_next_state(uint32_t node) { next_state(node); }
	static inline void t4_next_state(uint32_t node) { next_state(node); }
	// screen-space clip tests of the flat-triangle list (x 0..0xA00, y 0..0x6C0)
	static inline bool t1_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool t1_out_y(int16_t v) { return v < 0 || v > 0x6C0; }

	// the primitive-list renderers' helpers (mag090_tonberry.cpp parts t2 / t3)
	// GP0 0xE2 texture-window word from a RECT {x,y,w,h} (u16 each) at ctx+b
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

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl st_8366E0(uint32_t a1);
	uint32_t __cdecl st_836840(uint32_t a1, uint32_t a2);
	uint32_t __cdecl st_836950(uint32_t a1);
	uint32_t __cdecl st_836A10(uint32_t a1);
	uint32_t __cdecl st_836A40(uint32_t a1);
	uint32_t __cdecl st_836AC0(uint32_t a1);
	uint32_t __cdecl st_836B30(uint32_t a1);
	uint32_t __cdecl st_836BD0(uint32_t a1);
	uint32_t __cdecl st_836C20(uint32_t a1);
	uint32_t __cdecl st_836C80(uint32_t a1);
	uint32_t __cdecl st_836D10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl st_836DC0(uint32_t a1);
	uint32_t __cdecl st_836EB0(uint32_t a1);
	uint32_t __cdecl st_836FE0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl st_8372A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl st_8373D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl st_8375F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl st_837880(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl st_837D70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl st_838890(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl st_838DF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl st_8394C0(uint32_t a1);
	uint32_t __cdecl st_8395F0(uint32_t a1);
	uint32_t __cdecl st_839660(uint32_t a1);
	uint32_t __cdecl st_839740(uint32_t a1);
	uint32_t __cdecl st_839780(uint32_t a1);
	uint32_t __cdecl st_839790(uint32_t a1);
	uint32_t __cdecl st_8397E0(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag045_stare_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace stare
{

	// ====================================================================================
	// master, director, prim-model tasks (module code)
	// ====================================================================================

	// 0x8366E0 (MAG_045_sub_8366E0; Tonberry t_7624D0 with this module's table and queues, the 4th
	// queue replaced by the view-axis matrix 0x836840): MASTER task - camera copy, packet arena by
	// tick parity, bone follow, 11-state table {0x836930, 0x836940 (a_73A0D0), 0x836950 wait,
	// 0x836960 emitters (a_73A170), 0x839870 .. 0x8398F0, ret}, the queues (live task count -> node
	// +0x5E) with the view-axis matrix rebuilt between the stage queue and the prim-model queue
	uint32_t __cdecl st_8366E0(uint32_t a1)
	{
		g_mod = &MOD_045;
		// 30 fps layer: see mag045_stare_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x265574C) = 0x2793E58;
		MEM<uint32_t>(0x26544B8) = 0x2793E58;
		states[0] = 0x836930;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x836940;
		states[2] = 0x836950;
		states[3] = 0x836960;
		states[4] = 0x839870;
		states[5] = 0x8398B0;
		states[6] = 0x8398C0;
		states[7] = 0x8398D0;
		states[8] = 0x8398E0;
		states[9] = 0x8398F0;
		states[10] = 0x839910;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x26553C4);
			const uint32_t v2 = MEM<uint32_t>(0x2654FB4);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26544B4) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x26553C0);
			const uint32_t v2 = MEM<uint32_t>(0x2654FB0);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26544B4) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_DIRECTOR));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_STAGE));
		st_836840(0x1D97778, VIEW_MATRIX);
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PRIM));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_TINT));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x836840 (MAG_045_sub_836840): view-axis matrix a2 from the camera matrix a1: column 1 = the
	// camera's second row (+2/+8/+0xE) normalised (0x56BC50), column 0 = (y, -x, 0) of that axis
	// normalised, column 2 = their cross product (0x56BBF0) normalised; translation = the camera's
	// (+0x14..+0x1C). Used by the prim callback for records with flag 0x1000 (positions turned by
	// the view axes instead of the object's parent matrix)
	uint32_t __cdecl st_836840(uint32_t a1, uint32_t a2)
	{
		// [esp+8] frame: +0 A (3 x int32), +0x10 B, +0x20 C (every word written before it is read)
		alignas(4) uint8_t loc[0x30] = {};
		const uint32_t A = P(loc), B = A + 0x10, C = A + 0x20;
		S32(A, 0) = S16(a1, 2);
		S32(A, 4) = S16(a1, 8);
		S32(A, 8) = S16(a1, 0xE);
		sx::NormalizeVectorToFixedPoint(A, A);
		const uint32_t m = a2;
		const int32_t ax = S32(A, 0);
		const int32_t ay = S32(A, 4);
		const uint16_t az = U16(A, 8);
		U16(m, 2) = (uint16_t)ax;
		U16(m, 8) = (uint16_t)ay;
		S32(B, 4) = (int32_t)(0u - (uint32_t)ax);
		S32(B, 0) = ay;
		U16(m, 0xE) = az;
		S32(B, 8) = 0;
		sx::NormalizeVectorToFixedPoint(B, B);
		U16(m, 0) = U16(B, 0);
		U16(m, 6) = U16(B, 4);
		U16(m, 0xC) = U16(B, 8);
		sx::FixedPointCrossProduct(B, A, C);
		sx::NormalizeVectorToFixedPoint(C, C);
		U16(m, 4) = U16(C, 0);
		const uint32_t t0 = U32(a1, 0x14);
		U16(m, 0xA) = U16(C, 4);
		const uint32_t t1 = U32(a1, 0x18);
		U16(m, 0x10) = U16(C, 8);
		const uint32_t t2 = U32(a1, 0x1C);
		U32(m, 0x14) = t0;
		U32(m, 0x18) = t1;
		U32(m, 0x1C) = t2;
		return t2;
	}

	// 0x836AC0 (MAG_045_sub_836AC0; Psycho Blast pb_893500 with a 5-state table): TASK director -
	// phase bookkeeping (0x836B30) every tick, then its states {0x836B80 cue 1 (a_73B790),
	// 0x836BD0 sound once the phase runs, 0x836C20 damage cue after 0x20 ticks of the phase,
	// 0x836C40 finish (a_7475A0), ret}
	uint32_t __cdecl st_836AC0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x836B80;
		states[1] = 0x836BD0;
		states[2] = 0x836C20;
		states[3] = 0x836C40;
		states[4] = 0x836C60; // nullsub (ret)
		st_836B30(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x836C20 (sub_836C20): director state - 0x20 ticks into the phase sets the damage cue
	// (director block +0x48 = 1, read by the emitter's state 0x8397E0), next state
	uint32_t __cdecl st_836C20(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		if (S16(d, 0x46) < 0x20)
			return d; // void
		U16(d, 0x48) = 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x836C80 (sub_836C80; Psycho Blast pb_893830's shape): phase start (from 0x836B30) - phase 1
	// spawns the two eye stares (0x836D60, prim layout 0x17C4DA0 (0x1C4), +0x8A = 0 / 1: the
	// caster's points 0x15 / 0x16), the target's mark (0x839590, prim layout 0x17C6A14 (0x23C),
	// +0x80 = 1) and the stage darkening task 0x839690 (0x30 B, stage queue 0x2654598)
	uint32_t __cdecl st_836C80(uint32_t a1)
	{
		const int32_t ph = (int32_t)S16(MEM<uint32_t>(DIR_PTR), 0x40) - 1;
		if (ph != 0)
			return (uint32_t)ph;
		const uint32_t node = a1;
		const uint32_t e0 = st_836D10(node, ORIG_Eye, 0x17C4DA0, 0x1C4, 0, 0);
		U16(e0, 0x8A) = 0;
		const uint32_t e1 = st_836D10(node, ORIG_Eye, 0x17C4DA0, 0x1C4, 0, 0);
		U16(e1, 0x8A) = 1;
		st_836D10(node, ORIG_Mark, 0x17C6A14, 0x23C, 1, 0);
		return x::Effect_AddTaskAndInitFromCtx(Q_STAGE, ORIG_Darken, 0x30, node);
	}

	// 0x836D10 (sub_836D10; Doom dm_8C5860 with this module's pool): spawns task a2 (0x2D0 B, prim
	// queue 0x2654FA0, parent a1) and sets its model data +0x74 = a3, +0x78 = (s16)a4, +0x80 = a5,
	// +0x82 = a6; returns the node.
	uint32_t __cdecl st_836D10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PRIM, a2, 0x2D0, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		U32(t, 0x78) = (uint32_t)(int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// eye stare placement: the caster's (+0x2C) point 0x15 (+0x8A == 0) or 0x16 into +0x1C, moved
	// by (0, 0, -0x40) turned by the facing +0x62 (0x8DD770 / 0x8DD8A0, matrixMultiplyVector)
	static void eye_place(uint32_t node, uint32_t ent)
	{
		// [esp+0x10] frame: +0 SVECTOR (the +6 pad is never written: matrixMultiplyVector reads and
		// writes x, y, z only), +8 the 0x20-byte rotation matrix
		alignas(4) uint8_t loc[0x28] = {};
		const uint32_t V = P(loc);
		const uint32_t M = V + 8;
		x::GetEffectSpawnPosition(ent, U16(node, 0x8A) == 0 ? 0x15 : 0x16, 0, node + 0x1C);
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)S16(node, 0x62));
		U16(V, 0) = 0;
		U16(V, 2) = 0;
		U16(V, 4) = 0xFFC0;
		x::matrixMultiplyVector(M, V, V);
		const uint16_t vx = U16(V, 0);
		const uint16_t vy = U16(V, 2);
		const uint16_t vz = U16(V, 4);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + vx);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + vy);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + vz);
	}

	// 0x836DC0 (sub_836DC0): eye stare state 0 - decodes the prim layout (+0x74 -> +0x94, +0x78),
	// angles +0x60 = 0xF00, +0x62 = the caster's facing (entity +0x0E), placement (eye_place),
	// scale 1.0, depth offset +0x88 = -0x80, plays (0x836EB0), next state
	uint32_t __cdecl st_836DC0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = ENT(U8(node, 0x2C));
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U16(node, 0x60) = 0xF00;
		U16(node, 0x62) = U16(ent, 0xE);
		eye_place(node, ent);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFF80;
		st_836EB0(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x836EB0 (sub_836EB0; Boko b_72A720 with the module's blend buffer, no pause and the depth
	// offset +0x88 in the block): builds the prim-model effect matrix (rotations +0x62/+0x60/+0x64,
	// scale +0x50, position +0x1C, camera) and the 0x5C parameter block, then plays the prim-model
	// layout +0x94 with the draw callback 0x836FE0; returns the player result (0 = play finished)
	uint32_t __cdecl st_836EB0(uint32_t a1)
	{
		// stack block (0x5C bytes) handed to the callback: +0x00 Mat4x3 effect matrix,
		// +0x38 = node+0x70, +0x44 = node+0x7C, +0x48 = 0x2655428 (vertex lerp scratch),
		// +0x4C..+0x58 words = node +0x8C/+0x8E/+0x90/+0x92/+0x86/+0x80/+0x88. +0x20..+0x37,
		// +0x3C..+0x43, +0x5A are never written (the callback 0x836FE0 does not read them)
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
		// 30 fps layer: see mag045_stare_held.inc
		FX_HELD(held_note_play(node);)
		return prim_play(node + 0x94, PRIM_CB, L, 0);
	}

	// 0x836FE0 (sub_836FE0; Psycho Blast pb_893B20's shape): prim-model player draw callback
	// (layout a1, record a2, block a3) - picks the object's vertex frame (lerped by
	// MAG_017_sub_701390 into the block's buffer +0x48), builds its matrix (rotation 0x7015B0 with
	// record flag 0x40000, else 0x701310; position turned by the view-axis matrix 0x2655758 and that
	// matrix applied with flag 0x1000, not turned with flag 0x200, else turned by the block's
	// matrix and the block applied; scale3DMatrix), fills a 0x68-byte Field_Alloc render header
	// (flags 0x2030, 0x20F0 with alpha + colour, depth offset block +0x58, scale words 0x100) and
	// draws it with 0x8372A0 into OT base+0x44 (shift 2) at the frame arena cursor 0x1D8E054
	uint32_t __cdecl st_836FE0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position (+6 pad unwritten, never read), +8 scale (3 x int32; the
		// flag-0x1000 path's view-axis position first), +0x18 Mat4x3 object matrix (+0x2C translation)
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

		if ((U32(rec, 4) & 0x40000) != 0)
			sx::MAG_063_sub_7015B0(rec + 0x10, M);
		else
			x::MAG_017_sub_701310(rec + 0x10, M);
		const uint32_t flags0 = U32(rec, 4);
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		U16(V, 0) = vx;
		U16(V, 2) = vy;
		U16(V, 4) = vz;
		if ((flags0 & 0x1000) != 0)
		{
			// record flag 0x1000: position turned by the view-axis matrix, which is then applied
			x::GTE_SetRotMatrix(VIEW_MATRIX);
			x::GTE_LoadV0(V);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(S);
			const int32_t t0 = S32(S, 0);
			const int32_t t1 = S32(S, 4);
			const int32_t t2 = S32(S, 8);
			S32(M, 0x14) = t0;
			S32(M, 0x18) = t1;
			S32(M, 0x1C) = t2;
			x::GTE_MatrixMultiply(VIEW_MATRIX, M);
		}
		else if ((flags0 & 0x200) != 0)
		{
			// record flag 0x200: position not rotated by the parent (no GTE_MatrixMultiply either)
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
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0x14) = 0x2030;
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			U32(hdr, 0x14) = 0x20F0;
			U32(hdr, 8) = U32(rec, 0x20);   // colour
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
		MEM<uint32_t>(FRAME_CURSOR) = st_8372A0(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x8394C0 (sub_8394C0): eye stare state 1 - follows the caster's point (eye_place with the
	// facing kept in +0x62) and plays (0x836EB0); when the play ended: finished (+0x26 bit 0),
	// next state
	uint32_t __cdecl st_8394C0(uint32_t a1)
	{
		const uint32_t node = a1;
		eye_place(node, ENT(U8(node, 0x2C)));
		const uint32_t r = st_836EB0(node);
		if (r != 0)
			return r;
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8395F0 (sub_8395F0): target mark state 0 - from tick 0xF: decodes the prim layout (+0x74
	// -> +0x94, +0x78), position = the target's (+0x2D) anchor 0xF1 (0x502170), scale 1.0, depth
	// offset +0x88 = -0x80, plays (0x836EB0), next state
	uint32_t __cdecl st_8395F0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) <= 0xE)
			return node; // void
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		x::GetEffectSpawnPosition(ENT(U8(node, 0x2D)), 0xF1, 0, node + 0x1C);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U16(node, 0x88) = 0xFF80;
		st_836EB0(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x839740 (sub_839740; = modules 047 / 055 / 072): stage darkening state - level +0x1C +=
	// 0x200 up to 0x600 (then next state); writes the level to the 4 words 0x1D98992 + k * 0x2C
	uint32_t __cdecl st_839740(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x200);
		if (S16(a1, 0x1C) >= 0x600)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x1C) = 0x600;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x839780 (sub_839780; = modules 055 / 072): stage darkening state - holds until tick 0x29,
	// next state
	uint32_t __cdecl st_839780(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x28)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// ====================================================================================
	// library functions of this copy (adapted from the matching ports)
	// ====================================================================================

	// 0x762650 (module 090 MAG_090_sub_762650): master state - waits until no child task is alive
	// (+0x28 == 0), then next state.
	uint32_t __cdecl st_836950(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x893450 (copy of rt_8A36A0 0x8A36A0: copy of d_8B6D10 0x8B6D10: MAG_014_sub_8B6D10): emitter state 0 - sets up the director block (0x893480), spawns the director
	// 0x893500 (0x48 bytes, director queue), next state
	uint32_t __cdecl st_836A10(uint32_t a1)
	{
		st_836A40(a1);
		x::Effect_AddTaskAndInitFromCtx(0x2654490, 0x836AC0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x893480 (MAG_023_sub_893480): emitter a1 - clears the director block [0x1614284] (0x54 bytes),
	// +8 / +0xC = the target's (+0x2D) position words, origin +0..+4 = 0, facing +0x4A = the angle of
	// the target seen from the origin - 0x800 (12 bits)
	uint32_t __cdecl st_836A40(uint32_t a1)
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

	// 0x7328B0 (module 096 sub_7328B0): director phase bookkeeping - ++ticks in phase; a new
	// requested phase (+0x42) becomes current (+0x40, timer reset, m_733E90 spawns its actors);
	// the queued phase (+0x44) becomes the requested one.
	uint32_t __cdecl st_836B30(uint32_t a1)
	{
		uint32_t d = MEM<uint32_t>(DIR_PTR);
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			st_836C80(a1);
			d = MEM<uint32_t>(DIR_PTR);
		}
		const uint16_t next = U16(d, 0x44);
		if (U16(d, 0x42) != next)
		{
			U16(d, 0x42) = next;
			a_73A6D0();  // nullsub (pushed a1, unused)
		}
		return 0; // void
	}

	// 0x8C54E0 (copy of s_73B7B0 0x73B7B0: module; Siren sub_73B7B0): director state: once cut >= 1 runs, plays sound effect
	// 0x1634F24 (BdPlaySE, mode 0), advances the state
	uint32_t __cdecl st_836BD0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(1);
		if (r == 0) return 0;
		x::BdPlaySE(0x15C3580, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x764540 (module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri st_8373D0, st_8375F0, st_837880, st_837D70, a_73D770, a_73D9C0, st_838890,
	// st_838DF0; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl st_8372A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		static const Draw lists[8] = { st_8373D0, st_8375F0, st_837880, st_837D70, a_73D770, a_73D9C0, st_838890, st_838DF0 };
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

	// 0x764670 (module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl st_8373D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x764890 (module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl st_8375F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x764B20 (module 090 sub_764B20): draws the textured-triangle list of render context a1
	// (count, then records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage,
	// uv2 in the high half of +8): RTPT, rejects on GTE FLAG / back-face / fully off-screen, optional
	// depth cue, emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ(AVSZ3) + bias) >> a3.
	// With a UV scroll (+0x18/+0x1A) the texel bytes are scrolled (wrapped by +0x28/+0x2A) and the
	// poly is bracketed by two 0xE2 texture-window prims (0xC B each, windows +0x1C / +0x24);
	// without one a DR_MODE prim (0xC B, tpage abr 1) is inserted before the poly.
	uint32_t __cdecl st_837880(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x765010 (module 090 sub_765010): draws the textured-quad list of render context a1 (count,
	// then records of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16):
	// RTPT + RTPS, rejects on GTE FLAG / back-face / fully off-screen, optional depth cue, emits one
	// POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. With a UV scroll the texel
	// bytes are scrolled and the poly is bracketed by two 0xE2 texture-window prims; without one a
	// DR_MODE prim (tpage abr 1) is inserted before the poly.
	uint32_t __cdecl st_837D70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x765B30 (module 090 sub_765B30): prim-list block "Gouraud-textured triangles, UV scroll":
	// 0x1C-byte records (code/rgb0 +0, vertex indices +4/+6/+8, uv2 +0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, rgb1 +0x14, rgb2 +0x18) -> POLY_GT3 (tag 0x09000000) with RTPT, back-face
	// (NCLIP, unless ctx+0x14 bit 0x20) and off-screen culling, optional lighting (bit 0x80),
	// z = OTZ + ctx+0x10 (>= 0) >> a3 into OT a2. When the scroll ctx+0x18/+0x1A (u/v) is set, the
	// UVs are scrolled (wrapped by ctx+0x28/+0x2A) and the poly is framed by two E2 texture-window
	// packets (ctx+0x1C set, ctx+0x24 restore); else followed by one draw-mode packet (tpage abr 1).
	// Returns the new packet cursor; ctx+0x2C advances past the block.
	uint32_t __cdecl st_838890(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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

	// 0x766090 (module 090 sub_766090): prim-list block "Gouraud-textured quads, UV scroll":
	// 0x24-byte records (code/rgb0 +0, vertex indices +4/+6/+8/+0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, uv2 +0x14 / uv3 +0x16, rgb1..3 +0x18/+0x1C/+0x20) -> POLY_GT4 (tag
	// 0x0C000000), RTPT + RTPS for the 4th corner, AVSZ4, same culling / lighting / z / scroll /
	// texture-window framing as 0x765B30. Returns the new packet cursor; ctx+0x2C advances.
	uint32_t __cdecl st_838DF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl st_839660(uint32_t a1)
	{
		const uint32_t r = st_836EB0(a1);
		if (r != 0)
			return r;
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x883040 (MAG_028_sub_883040; = Drain r_856920, copy of m_733C90 0x733C90; module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl st_839790(uint32_t a1)
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

	// 0x768370 (module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl st_8397E0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
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

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x836930, (void *)a_73A0D0, "045 MAG_045_sub_836930" },
		{ 0x836940, (void *)a_73A0D0, "045 MAG_045_sub_836940" },
		{ 0x836960, (void *)a_73A170, "045 MAG_045_sub_836960" },
		{ 0x836990, (void *)a_73A1A0, "045 MAG_045_sub_836990" },
		{ 0x836B80, (void *)a_73B790, "045 sub_836B80" },
		{ 0x836BA0, (void *)a_73B640, "045 sub_836BA0" },
		{ 0x836C00, (void *)a_73B7E0, "045 sub_836C00" },
		{ 0x836C40, (void *)a_7475A0, "045 sub_836C40" },
		{ 0x836C60, (void *)a_73A6D0, "045 nullsub_1577" },
		{ 0x836C70, (void *)a_73A6D0, "045 nullsub_1578" },
		{ 0x836D60, (void *)a_73A380, "045 sub_836D60" },
		{ 0x838360, (void *)a_73D770, "045 sub_838360" },
		{ 0x8385B0, (void *)a_73D9C0, "045 sub_8385B0" },
		{ 0x839580, (void *)a_73A6D0, "045 nullsub_1579" },
		{ 0x839590, (void *)a_73A380, "045 sub_839590" },
		{ 0x839680, (void *)a_73A6D0, "045 nullsub_1580" },
		{ 0x839690, (void *)a_73F110, "045 sub_839690" },
		{ 0x839700, (void *)a_7335D0, "045 sub_839700" },
		{ 0x8397D0, (void *)a_73A6D0, "045 nullsub_1581" },
		{ 0x839820, (void *)a_7474B0, "045 MAG_045_sub_839820" },
		{ 0x839840, (void *)a_7474D0, "045 MAG_045_sub_839840" },
		{ 0x839860, (void *)a_73A6D0, "045 nullsub_1582" },
		{ 0x839870, (void *)a_747500, "045 MAG_045_sub_839870" },
		{ 0x8398B0, (void *)a_73A0D0, "045 MAG_045_sub_8398B0" },
		{ 0x8398C0, (void *)a_747550, "045 MAG_045_sub_8398C0" },
		{ 0x8398D0, (void *)a_73A0D0, "045 MAG_045_sub_8398D0" },
		{ 0x8398E0, (void *)a_73A0D0, "045 MAG_045_sub_8398E0" },
		{ 0x8398F0, (void *)a_7475A0, "045 MAG_045_sub_8398F0" },
		{ 0x839910, (void *)a_73A6D0, "045 nullsub_1576" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ 0x8366E0, (void *)st_8366E0, "045 st_8366E0" },
		{ 0x836840, (void *)st_836840, "045 st_836840" },
		{ 0x836950, (void *)st_836950, "045 st_836950" },
		{ 0x836A10, (void *)st_836A10, "045 st_836A10" },
		{ 0x836A40, (void *)st_836A40, "045 st_836A40" },
		{ 0x836AC0, (void *)st_836AC0, "045 st_836AC0" },
		{ 0x836B30, (void *)st_836B30, "045 st_836B30" },
		{ 0x836BD0, (void *)st_836BD0, "045 st_836BD0" },
		{ 0x836C20, (void *)st_836C20, "045 st_836C20" },
		{ 0x836C80, (void *)st_836C80, "045 st_836C80" },
		{ 0x836D10, (void *)st_836D10, "045 st_836D10" },
		{ 0x836DC0, (void *)st_836DC0, "045 st_836DC0" },
		{ 0x836EB0, (void *)st_836EB0, "045 st_836EB0" },
		{ 0x836FE0, (void *)st_836FE0, "045 st_836FE0" },
		{ 0x8372A0, (void *)st_8372A0, "045 st_8372A0" },
		{ 0x8373D0, (void *)st_8373D0, "045 st_8373D0" },
		{ 0x8375F0, (void *)st_8375F0, "045 st_8375F0" },
		{ 0x837880, (void *)st_837880, "045 st_837880" },
		{ 0x837D70, (void *)st_837D70, "045 st_837D70" },
		{ 0x838890, (void *)st_838890, "045 st_838890" },
		{ 0x838DF0, (void *)st_838DF0, "045 st_838DF0" },
		{ 0x8394C0, (void *)st_8394C0, "045 st_8394C0" },
		{ 0x8395F0, (void *)st_8395F0, "045 st_8395F0" },
		{ 0x839660, (void *)st_839660, "045 st_839660" },
		{ 0x839740, (void *)st_839740, "045 st_839740" },
		{ 0x839780, (void *)st_839780, "045 st_839780" },
		{ 0x839790, (void *)st_839790, "045 st_839790" },
		{ 0x8397E0, (void *)st_8397E0, "045 st_8397E0" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag045_stare()
	{
		act::register_module(45);
		for (const act::stare::ModPort *p = act::stare::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(45, p->addr, p->port, p->name);
		for (const act::stare::ModPort *p = act::stare::PORTS; p->addr; p++)
			act::register_module_port(45, p->addr, p->port, p->name);
		// 30 fps layer: see mag045_stare_held.inc
		FX_HELD(register_mag045_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag045_stare_held.inc"
#endif
