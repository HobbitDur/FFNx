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

// Effect 79: Ultimecia's final form dies (enemy attack 351 of kernel.bin, no name; Ultimecia's final
// form c0m122; no other kernel.bin entry uses effect 79; MAG_079_GRIEVER_DEATH, data files
// magic/mag078.tim + battle/MAG078_B.1S0..1S3 / .1T0 / .9M0): a copy of the actor/heal effect library
// (see act_engine.h) built like Siren (mag095_siren.cpp: master + director + load script, the
// prim-model sequence tasks of Doom / Psycho Blast, the actor system) plus the module's own parts.
//
// Setup MAG_079_GRIEVER_DEATH_Init 0x7B4BD0 (runs once, not ported; file loader 0x7B4BB0 = the
// texture file 0x1581F88 -> [0x25DF338], TIM uploaded by the setup unless the cast context's flag +1
// bit 0, camera animation 0x1581BCC; packet arenas at file + 0x8000 / + 0x10000 (0x25EA340/0x25EA344,
// 0x25E9738/0x25E973C) by tick parity, the file arena cursor 0x25EB3B8 = file + 0x10000) creates the
// root queue 0x25E50F0 with the master and nine task pools.
//   Master (0x7B4D80 = Siren's master 0x739F40 with 13 states) - camera copy 0x2793E58, packet
//     arenas by tick parity (sprite cursor 0x25DF33C), bone follow, states: 0x7B4F40 reserves the
//     actor pools after the file arena (0x870 + 0x9D80 bytes, cleared by 0x7B4F80), 0x7B4FC0 starts
//     the director 0x7B4FF0 (Siren's load-script director), 0x7BE150 next action, 0x7BE1A0 loads
//     battle/MAG078_B.9M0 into the magic buffer once nothing runs any more (callback 0x7BE1E0),
//     0x7BE1F0 hands it to 0x501A20, 0x7BE220 music channel volume, 0x7BE240 / 0x7BE270 unload wait,
//     0x7BE280 finish.
//   Director (0x7B55B0, Tonberry's 0x762C60 with 11 states) - cuts 1..3: claims a voice + music
//     fade, hides the party models (flag bit 2) for the whole show, the damage cue at cut frame 0x5A
//     (read by 0x7BE0C0: ApplyActionResultToTarget), restores the party and releases the voice.
//     Cut 1 starts the caster-model task 0x7BCF10, the debris thrower 0x7BC610 and the light level
//     task 0x7BCD50; cut 2 the actor system 0x7B82C0 (data 0x176A958), the flake thrower 0x7BCB40,
//     the spark thrower 0x7BBC20, three prim-model sequence tasks (0x7B5B80 / 0x7B80B0 / 0x7B81C0,
//     sequence files MAG078_B.1S0..1S2) and the light fade-out 0x7BCE40.
//   Caster-model task (0x7BCF10, 11 states) - copies the caster's model into the block 0x25DF348
//     (0x7BD890), hides the caster, then morphs the copy towards a ball (0x7BD110: Raldo's ball
//     renderer with a depth bias), throws it (velocity 0x25DF3D8.., gravity 0x25DF40E) and, while
//     drawn, WRITES THE CASTER ENTITY's position (+0x1C/+0x1E/+0x20 and +0x54/+0x58/+0x5C) every tick;
//     state 7 (0x7BDEF0) WRITES THE TARGET ENTITY's position (anchor + (0x80, -0x20, -0x150)) and
//     flag word, states 6..9 fade the target's / caster's colour (0x7BDDF0) and set / clear the
//     target's scale pointer +0x30 (0x25DF3F4); the light records 0x1D98992 + k * 0x2C (k < 4) are
//     written by 0x7BCD50 / 0x7BCE40.
//   Particles: sparks (0x7BBDF0, 2 per task), debris (0x7BC840, 2 per task, gravity), flakes
//     (0x7BCC80) - sprite sequences drawn by 0x7BBEB0 / 0x7BC950 (module sprite cursor 0x25DF33C).
//   The actor systems (0x7B82C0, a_73F990 family) and prim-model sequences (0x7B5C50 = a_73C280,
//     callback 0x7B5D80) are the library's.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace udeath
{
	static const Mod MOD_079 = { "ultideath", 79, 0x7B4BB0, 0x7BE2B0,
		{ 0x0, 0x0, 0x1580B20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25DF338, 0x0, 0x0, 0x0, 0x25E5024, 0x0, 0x0, 0x25E8954, 0x0, 0x0, 0x0, 0x0, 0x25E9F30, 0x0, 0x25EA1D0, 0x0, 0x0, 0x25EA348, 0x25EAD84, 0x0, 0x25EAF30, 0x0, 0x25EAFA8, 0x25EB390, 0x0, 0x25EB3AC, 0x25EB3B4, 0x25EB3BC },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7B4FF0, 0x7B5070, 0x0, 0x7B53C0, 0x7B53F0, 0x7B5420, 0x7B5430, 0x7B5440, 0x7B5450, 0x7B5460, 0x0, 0x0, 0x7B57B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7B56E0, 0x0, 0x7B5730, 0x7B5D80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7B8500, 0x7B85E0, 0x7B8780, 0x7B8850, 0x7B8970, 0x7B8BD0, 0x0, 0x0, 0x7B98F0, 0x7B9940, 0x0, 0x0, 0x7BA230, 0x0, 0x0, 0x0, 0x7BAC10, 0x7BAC40, 0x7BAD20, 0x0, 0x0, 0x0, 0x7BB650, 0x0, 0x0, 0x7BC110, 0x0, 0x7BE0C0, 0x7BE100, 0x7BE120, 0x7BE140, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t DIR_PTR = 0x1580B20;        // pointer to the director block (cut +0x40/+0x42/+0x44, frame +0x46)
	static const uint32_t SPRITE_CURSOR = 0x25DF33C;  // module sprite packet cursor (tick parity: file + 0x8000 / + 0)
	static const uint32_t MODEL_BLOCK = 0x25DF348;    // the caster-model copy block (0xD4 bytes)
	static const uint32_t PHASE_DATA = DIR_PTR;       // (the library's name of the director pointer)
	static const uint32_t PACKET_CURSOR = SPRITE_CURSOR;
	static const uint32_t SCRATCH_SP = 0x25EB3AC;     // module scratch stack pointer (below the camera copy 0x2793E58)
	static const uint32_t ACTOR_STATE = 0x25EB3BC;    // current actor system state block


	// RESIDUE (UNINIT 0x7BC782): the debris burst 0x7BC720 in its bone mode (task 0x7BC610 state 2) turns the debris
	// velocities by a stack matrix whose rotation (+0x00..+0x11, the task's entry esp - 0x50 .. - 0x3F) no code of the
	// task writes (0x8DE8F0 writes only the translation): the original reads what the previous code left there. The
	// game's executor 0x508420 calls every task of every queue of the master at the same esp; the task run just before
	// 0x7BC610 is the caster-model task 0x7BCF10 (spawned just before it in the particle queue 0x25E5100), which leaves:
	//   kind 2 (it drew the model copy this tick): 0x7BD020's first call argument (the drawn entity +0x60), its saved edi
	//     (the 0x7BCF10 node), esi (the caster entity), ebp (the queue 0x25E5100) and ebx (the executor's count of the
	//     live tasks it ran before 0x7BCF10 in that queue);
	//   kind 1 (it waits for cut 2 in 0x7BD900): the 0x7BD921 return, the gate argument 2, the node and the caster above
	//     the first word, which then still holds the saved edi of the last GTE_MVMVA 0x460860 call of the emitter's
	//     MAG_001_CURE_Emitter_ComputeModelBounds 0x8DC870 bone loop (queue 0x25EA1D0, run earlier in the tick): the
	//     skeleton of the entity +0x2D + 0x50 + 0x30 * (bone count - 2);
	//   otherwise unknown (0 here).
	struct Residue
	{
		uint32_t node;     // the 0x7BCF10 node that wrote it, and its tick counter after that run
		uint16_t stamp;
		int kind;          // 0 = unknown, 1 = waiting for cut 2, 2 = model drawn
		uint32_t w[5];     // the words at entry esp - 0x50, - 0x4C, - 0x48, - 0x44, - 0x40
	};
	static Residue g_res = {};

	// the rotation words of 0x7BC720's matrix for the debris task `node` (when 0x7BCF10 ran just before it on this tick)
	static void res_rotation(uint32_t node, uint32_t L)
	{
		uint32_t prev = 0;
		for (uint32_t q = MEM<uint32_t>(0x25E5100); q != 0 && q != node; q = U32(q, 4))
			prev = q;
		if (g_res.kind == 0 || prev == 0 || prev != g_res.node || U16(prev, 0x24) != g_res.stamp)
			return;
		uint32_t w[5];
		memcpy(w, g_res.w, sizeof(w));
		if (g_res.kind == 1)
		{
			const uint32_t te = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
			const uint32_t m = U32(te, 0x64);
			const uint32_t sk = m ? U32(m, 0) : 0;
			w[0] = (sk && U8(sk, 0) >= 2) ? sk + 0x50 + 0x30 * ((uint32_t)U8(sk, 0) - 2) : 0;
		}
		memcpy((void *)L, w, 0x12);
	}

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

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl ud_7B4D80(uint32_t a1);
	uint32_t __cdecl ud_7B4F40(uint32_t a1);
	uint32_t __cdecl ud_7B4F80(void);
	uint32_t __cdecl ud_7B5070(uint32_t a1);
	uint32_t __cdecl ud_7B50C0(void);
	uint32_t __cdecl ud_7B5520(uint32_t a1);
	uint32_t __cdecl ud_7B55B0(uint32_t a1);
	uint32_t __cdecl ud_7B5650(uint32_t a1);
	uint32_t __cdecl ud_7B56A0(uint32_t a1);
	uint32_t __cdecl ud_7B5710(uint32_t a1);
	uint32_t __cdecl ud_7B5750(void);
	uint32_t __cdecl ud_7B57F0(uint32_t a1);
	uint32_t __cdecl ud_7B5880(uint32_t a1);
	uint32_t __cdecl ud_7B58E0(uint32_t a1);
	uint32_t __cdecl ud_7B5900(uint32_t a1);
	uint32_t __cdecl ud_7B5930(void);
	uint32_t __cdecl ud_7B59B0(uint32_t a1);
	uint32_t __cdecl ud_7B5B30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6);
	uint32_t __cdecl ud_7B5BE0(uint32_t a1);
	uint32_t __cdecl ud_7B5D80(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7B6020(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7B6150(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7B6370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7B6600(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7B6A80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7B7530(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7B7A30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7B8080(uint32_t a1);
	uint32_t __cdecl ud_7B8110(uint32_t a1);
	uint32_t __cdecl ud_7B8170(uint32_t a1);
	uint32_t __cdecl ud_7B8220(uint32_t a1);
	uint32_t __cdecl ud_7B8290(uint32_t a1);
	uint32_t __cdecl ud_7B8320(uint32_t a1);
	uint32_t __cdecl ud_7B8500(uint32_t a1);
	uint32_t __cdecl ud_7B8780(void);
	uint32_t __cdecl ud_7B8E30(uint32_t a1);
	uint32_t __cdecl ud_7B8EC0(void);
	uint32_t __cdecl ud_7B8F20(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7B9080(void);
	uint32_t __cdecl ud_7B9140(uint32_t a1);
	uint32_t __cdecl ud_7B91C0(void);
	uint32_t __cdecl ud_7B9200(uint32_t a1);
	uint32_t __cdecl ud_7BA230(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7BA340(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7BA3C0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7BA3F0(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7BA490(uint32_t a1);
	uint32_t __cdecl ud_7BA510(uint32_t a1);
	uint32_t __cdecl ud_7BA590(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7BAD70(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7BAF90(uint32_t a1);
	uint32_t __cdecl ud_7BB680(uint32_t a1);
	uint32_t __cdecl ud_7BB970(uint32_t a1);
	uint32_t __cdecl ud_7BBAD0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7BBB60(void);
	uint32_t __cdecl ud_7BBBF0(uint32_t a1);
	uint32_t __cdecl ud_7BBC80(uint32_t a1);
	uint32_t __cdecl ud_7BBCA0(uint32_t a1);
	uint32_t __cdecl ud_7BBCD0(uint32_t a1);
	uint32_t __cdecl ud_7BBDF0(uint32_t a1);
	uint32_t __cdecl ud_7BBEB0(uint32_t a1);
	uint32_t __cdecl ud_7BC580(uint32_t a1);
	uint32_t __cdecl ud_7BC5A0(uint32_t a1);
	uint32_t __cdecl ud_7BC610(uint32_t a1);
	uint32_t __cdecl ud_7BC670(uint32_t a1);
	uint32_t __cdecl ud_7BC6B0(uint32_t a1);
	uint32_t __cdecl ud_7BC720(uint32_t a1, uint32_t a2, uint32_t a3);
	uint32_t __cdecl ud_7BC840(uint32_t a1);
	uint32_t __cdecl ud_7BC950(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7BCAA0(uint32_t a1);
	uint32_t __cdecl ud_7BCAC0(uint32_t a1);
	uint32_t __cdecl ud_7BCAF0(uint32_t a1);
	uint32_t __cdecl ud_7BCBA0(uint32_t a1);
	uint32_t __cdecl ud_7BCBC0(uint32_t a1);
	uint32_t __cdecl ud_7BCBF0(uint32_t a1);
	uint32_t __cdecl ud_7BCC80(uint32_t a1);
	uint32_t __cdecl ud_7BCCF0(uint32_t a1);
	uint32_t __cdecl ud_7BCD10(uint32_t a1);
	uint32_t __cdecl ud_7BCDF0(uint32_t a1);
	uint32_t __cdecl ud_7BCEA0(uint32_t a1);
	uint32_t __cdecl ud_7BCEC0(uint32_t a1);
	uint32_t __cdecl ud_7BCF10(uint32_t a1);
	uint32_t __cdecl ud_7BD020(uint32_t a1);
	uint32_t __cdecl ud_7BD110(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
	uint32_t __cdecl ud_7BD800(uint32_t a1);
	uint32_t __cdecl ud_7BD890(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5);
	uint32_t __cdecl ud_7BD900(uint32_t a1);
	uint32_t __cdecl ud_7BD9B0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7BDA60(uint32_t a1);
	uint32_t __cdecl ud_7BDB30(uint32_t a1);
	uint32_t __cdecl ud_7BDB90(uint32_t a1);
	uint32_t __cdecl ud_7BDBE0(uint32_t a1, uint32_t a2);
	uint32_t __cdecl ud_7BDCB0(uint32_t a1);
	uint32_t __cdecl ud_7BDD00(uint32_t a1);
	uint32_t __cdecl ud_7BDD40(uint32_t a1);
	uint32_t __cdecl ud_7BDD70(uint32_t a1);
	uint32_t __cdecl ud_7BDEF0(uint32_t a1);
	uint32_t __cdecl ud_7BDF90(uint32_t a1);
	uint32_t __cdecl ud_7BE050(uint32_t a1);
	uint32_t __cdecl ud_7BE0C0(uint32_t a1);
	uint32_t __cdecl ud_7BE1A0(uint32_t a1);
	uint32_t __cdecl ud_7BE1E0(void);
	uint32_t __cdecl ud_7BE1F0(uint32_t a1);
	uint32_t __cdecl ud_7BE220(uint32_t a1);
}
}
}

#ifdef FF8_FX_HELD
#include "mag079_ultimecia_final_death_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace udeath
{
	// ====================================================================================
	// the module's functions (by address): hand ports, and the library functions whose code
	// matches a Doom / Death / Siren / Tonberry / Raijin Attack / Sigh / Magma Breath / Esuna /
	// Mighty Guard port up to the module's addresses and callees (the port's text with the
	// module's addresses; their comments describe the source original)
	// ====================================================================================

	// 0x7B4D80 (MAG_079_sub_7B4D80): MASTER task (Siren a_739F40 with 13 states) - camera matrix copied
	// to 0x2793E58 (pointers 0x25EB3AC / 0x25E5024), packet arenas by tick parity (+0x5C: odd = 0x25EA344
	// / 0x25E973C, even = 0x25EA340 / 0x25E9738 -> 0x25DF33C sprites / 0x25E5020), Effect_UpdateTargetPosFromBones,
	// its state table {0x7B4F20, 0x7B4F30 next, 0x7B4F40 wait + set-up, 0x7B4FC0 director, 0x7BE150 next
	// action, 0x7BE190 next, 0x7BE1A0 load .9M0, 0x7BE1F0 hand over, 0x7BE220 music, 0x7BE240 / 0x7BE270
	// unload wait, 0x7BE280 finish, 0x7BE2A0 ret}, then the 9 task queues (live count -> +0x5E)
	uint32_t __cdecl ud_7B4D80(uint32_t a1)
	{
		g_mod = &MOD_079;
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_master();)
		// rep movsd: camera matrix (Mat4x3, 8 dwords) -> shared snapshot 0x2793E58
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);
		MEM<uint32_t>(0x25EB3AC) = 0x2793E58;
		MEM<uint32_t>(0x25E5024) = 0x2793E58;
		uint32_t states[13] = {
			0x7B4F20, 0x7B4F30, 0x7B4F40, 0x7B4FC0, 0x7BE150, 0x7BE190, 0x7BE1A0,
			0x7BE1F0, 0x7BE220, 0x7BE240, 0x7BE270, 0x7BE280, 0x7BE2A0,
		};
		if (U8(a1, 0x5C) & 1) // +0x5C tick counter parity -> buffer 1
		{
			const uint32_t v1 = MEM<uint32_t>(0x25EA344);
			const uint32_t v2 = MEM<uint32_t>(0x25E973C);
			MEM<uint32_t>(0x25DF33C) = v1;
			MEM<uint32_t>(0x25E5020) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x25EA340);
			const uint32_t v2 = MEM<uint32_t>(0x25E9738);
			MEM<uint32_t>(0x25DF33C) = v1;
			MEM<uint32_t>(0x25E5020) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(a1);
		callp(states[S8(a1, 0x29)], a1); // +0x29 state index

		U16(a1, 0x5E) = 0; // live task count of this tick
		MEM<uint16_t>(0x25EB390) = 0;
		MEM<uint16_t>(0x25EAF30) = 0;
		static const uint32_t queues[9] = { 0x25EA1D0, 0x25DF4F8, 0x25EB398, 0x25EAF20, 0x25E9F30,
			0x25E5100, 0x25E8958, 0x25E9728, 0x25EA1C0 };
		for (int i = 0; i < 9; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(a1, 0x5E) = (uint16_t)(U16(a1, 0x5E) + (uint16_t)n);
		}
		const uint8_t status = U8(a1, 0x26);
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) + 1);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1); // frame counter
		if ((status & 1) && U8(a1, 0x28) == 0) // finished and no live children
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x8B6BE0 (MAG_014_sub_8B6BE0): master state - once no child is alive: the two actor pools are
	// carved from the file arena cursor [0x2742870] (0x870 + 0x9D80 bytes) and cleared, next state
	uint32_t __cdecl ud_7B4F40(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		uint32_t p = MEM<uint32_t>(0x25EB3B8);
		MEM<uint32_t>(0x25EAF34) = p;
		p += 0x870;
		MEM<uint32_t>(0x25EB3B0) = p;
		p += 0x9D80;
		MEM<uint32_t>(0x25EB3B8) = p;
		ud_7B4F80();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x8B6C20 (MAG_014_sub_8B6C20): clears the two actor pools (0x870 bytes at [0x2742824], 0x9D80
	// at [0x2742868]) and the pool cursors / counters
	uint32_t __cdecl ud_7B4F80(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25EAF34), 0x870);
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x25EB3B0), 0x9D80);
		MEM<uint16_t>(0x25E8950) = 0;
		MEM<uint16_t>(0x25EAF40) = 0;
		MEM<uint16_t>(0x25EAD80) = 0;
		MEM<uint16_t>(0x25DF342) = 0;
		return 0; // void
	}

	// 0x8C4E10 (copy of t_762710 0x762710: module 090 MAG_090_sub_762710): master state - init director data (t_762BB0 +
	// a_7322A0), spawns task 0x762850 (0x30 B, queue 0x25A4BF0) and the director task 0x762C60
	// (0x48 B, queue 0x259EF90), next state.
	uint32_t __cdecl ud_7B5070(uint32_t a1)
	{
		ud_7B5520(a1);
		ud_7B50C0();
		x::Effect_AddTaskAndInitFromCtx(0x25EB398, 0x7B51C0, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x25DF4F8, 0x7B55B0, 0x48, a1);
		t1_next_state(a1);
		return 0; // void
	}

	// 0x7B50C0 (MAG_079_sub_7B50C0): initialises the load context 0x25EB3C0 (0x16C bytes,
	// MAG_007_sub_8DCC00; pointer in 0x25EAD84), the load-script cursor 0x25EA348 = 0x1581F18,
	// gives it a 0xA000-byte slice of the arena cursor 0x25EB3B8 (+0x100), its slot tables
	// (+0x104..+0x160: default 0x19F1018, +0x128/+0x12C/+0x130 = the three loaded sequence files)
	// and clears +0x164/+0x166
	uint32_t __cdecl ud_7B50C0(void)
	{
		MEM<uint32_t>(0x25EA348) = 0x1581F18;
		MEM<uint32_t>(0x25EAD84) = 0x25EB3C0;
		x::MAG_007_sub_8DCC00(0x25EB3C0, 0x16C);
		const uint32_t c = MEM<uint32_t>(0x25EAD84);
		const uint32_t arena = MEM<uint32_t>(0x25EB3B8);
		U32(c, 0x100) = arena;
		MEM<uint32_t>(0x25EB3B8) = arena + 0xA000;
		const uint32_t dflt = 0x19F1018;
		U32(c, 0x114) = dflt;
		U32(c, 0x118) = dflt;
		U32(c, 0x11C) = dflt;
		U32(c, 0x120) = dflt;
		U32(c, 0x104) = dflt;
		U32(c, 0x108) = dflt;
		U32(c, 0x10C) = dflt;
		U32(c, 0x110) = dflt;
		U32(c, 0x124) = dflt;
		U32(c, 0x128) = 0x19F24C4;
		U32(c, 0x12C) = 0x19F36C0;
		U32(c, 0x130) = 0x19F4880;
		for (int32_t o = 0x134; o <= 0x160; o += 4)
			U32(c, o) = dflt;
		U16(c, 0x164) = 0;
		U16(c, 0x166) = 0;
		return c;
	}

	// 0x8C52C0 (MAG_008_sub_8C52C0): phase data init - cleared (0x54 bytes), +0 = 0 / +8 = the
	// position of the node's target slot (+0x2D; entity +0x1C / +0x20), +0x4A = the facing angle
	// from +0 to +8 (CartesianToGameAngle - 0x800); the effect light matrix 0x1D99AB0 = rotation
	// about y by that angle
	uint32_t __cdecl ud_7B5520(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(PHASE_DATA), 0x54);
		const uint32_t d = MEM<uint32_t>(PHASE_DATA);
		const uint32_t ent = 0x1D972C0u + (uint32_t)U8(a1, 0x2D) * 0x9Cu;
		U32(d, 8) = U32(ent, 0x1C);
		U32(d, 0xC) = U32(ent, 0x20);
		U16(d, 0) = 0;
		U16(d, 2) = 0;
		U16(d, 4) = 0;
		const int16_t dx_ = (int16_t)(U16(d, 8) - U16(d, 0));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)dx_, (uint32_t)(int32_t)S16(d, 0xC));
		const uint32_t d2 = MEM<uint32_t>(PHASE_DATA);
		U16(d2, 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		x::MAG_022_sub_8DD770(0x1D99AB0);
		return x::MAG_022_sub_8DD8A0(0x1D99AB0, (uint32_t)(int32_t)S16(MEM<uint32_t>(PHASE_DATA), 0x4A));
	}

	// 0x7B55B0 (MAG_079_sub_7B55B0): DIRECTOR task - cut bookkeeping (0x7B5650), then its 11-state
	// table {0x7B56A0 claim voice + music fade, 0x7B5710 cut 1: hide the monsters, 0x7B5790 load
	// step, 0x7B57F0 toggle at cut frame 0x62, 0x7B5880 frame 0x63, 0x7B58A0 cut 2, 0x7B58C0,
	// 0x7B58E0 damage cue at frame 0x5A, 0x7B5900 cut 3: show the monsters + release the voice,
	// 0x7B5970 finish, 0x7B5990 ret}
	uint32_t __cdecl ud_7B55B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[11];
		states[0] = 0x7B56A0;
		states[1] = 0x7B5710;
		states[2] = 0x7B5790;
		states[3] = 0x7B57F0;
		states[4] = 0x7B5880;
		states[5] = 0x7B58A0;
		states[6] = 0x7B58C0;
		states[7] = 0x7B58E0;
		states[8] = 0x7B5900;
		states[9] = 0x7B5970;
		states[10] = 0x7B5990;
		ud_7B5650(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8C5410 (copy of t_762D10 0x762D10: module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl ud_7B5650(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			ud_7B59B0(a1);
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

	// 0x7B56A0 (MAG_079_sub_7B56A0): director state 0 - at cut 1 (0x7B56E0 gate): claims a voice
	// slot (BdSound_ClaimVoiceSlot(0x176E064, 1, 0x80) -> director +0x52), music volume fade
	// (0x46BBC0(0, 0x5A, 0)), next state
	uint32_t __cdecl ud_7B56A0(uint32_t a1)
	{
		if (a_73B640(1) == 0)
			return 0;
		const uint32_t v = x::BdSound_ClaimVoiceSlot(0x176E064, 1, 0x80);
		U16(MEM<uint32_t>(0x1580B20), 0x52) = (uint16_t)v;
		gx::Music_SetVolumeTrans(0, 0x5A, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B5710 (sub_7B5710): director state 1 - once cut >= 1: hides the monsters (0x7B5750), next state
	uint32_t __cdecl ud_7B5710(uint32_t a1)
	{
		if (a_73B7E0(1) == 0)
			return 0;
		ud_7B5750();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B5750 (sub_7B5750): for the battle entities 0..2 (0x1D972C0, stride 0x9C, 3 entries) with flag
	// bit 1: saves the flag word to director +0x20[i] and sets flag bit 2
	uint32_t __cdecl ud_7B5750(void)
	{
		uint32_t save = MEM<uint32_t>(0x1580B20) + 0x20;
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

	// 0x7B57F0 (sub_7B57F0): director state 3 - load context +0x166 == 0: toggle off (0x7B5830(0)),
	// next state; else from cut frame 0x62 toggle on (0x7B5830(1))
	uint32_t __cdecl ud_7B57F0(uint32_t a1)
	{
		const uint32_t tab = MEM<uint32_t>(0x25EAD84);
		if (U16(tab, 0x166) == 0)
		{
			a_73BB60(0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return a1;
		}
		const uint32_t blk = MEM<uint32_t>(0x1580B20);
		if (S16(blk, 0x46) < 0x62)
			return blk;
		return a_73BB60(1);
	}

	// 0x7B5880 (sub_7B5880): director state 4 - next state once the cut frame (+0x46) reaches 0x63
	uint32_t __cdecl ud_7B5880(uint32_t a1)
	{
		const uint32_t blk = MEM<uint32_t>(0x1580B20);
		if (S16(blk, 0x46) < 0x63)
			return blk;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B58E0 (sub_7B58E0): director state 7 - from cut frame 0x5A: the damage cue (director
	// +0x48 = 1), next state
	uint32_t __cdecl ud_7B58E0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1580B20);
		if (S16(d, 0x46) < 0x5A)
			return d;
		U16(d, 0x48) = 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B5900 (sub_7B5900): director state 8 - at cut 3 (0x7B56E0 gate): restores the entities'
	// flag bit 2 (0x7B5930), releases the voice slot of director +0x52 (sub_4A2940), next state
	uint32_t __cdecl ud_7B5900(uint32_t a1)
	{
		if (a_73B640(3) == 0)
			return 0;
		ud_7B5930();
		x::sub_4A2940((uint32_t)(int32_t)S16(MEM<uint32_t>(0x1580B20), 0x52));
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7B5930 (sub_7B5930): restores flag bit 2 of the battle entities 0..2 (flag bit 1 set) from
	// the words saved by 0x7B5750
	uint32_t __cdecl ud_7B5930(void)
	{
		uint32_t save = MEM<uint32_t>(0x1580B20) + 0x20;
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

	// 0x7B59B0 (sub_7B59B0): director cut-change hook (0x7B5650 calls it when the cut +0x40 changes):
	// cut 1 starts the tasks 0x7BCF10, 0x7BC610, 0x7BCD50 (0xB4 bytes, queue 0x25E5100); cut 2 the
	// actor system 0x7B82C0 (0x7B5AE0: data 0x176A958, mode 0xF), the tasks 0x7BCB40 / 0x7BBC20,
	// the three prim-model sequence tasks 0x7B5B80 / 0x7B80B0 / 0x7B81C0 (0x7B5B30: sequence files
	// of the load context +0x124 / +0x128 / +0x12C, layouts 0xEC / 0x134 / 0x134, ids 0 / 1 / 2)
	// and the task 0x7BCE40
	uint32_t __cdecl ud_7B59B0(uint32_t a1)
	{
		const int32_t cut = (int32_t)S16(MEM<uint32_t>(0x1580B20), 0x40);
		if (cut == 1)
		{
			x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BCF10, 0xB4, a1);
			x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BC610, 0xB4, a1);
			return x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BCD50, 0xB4, a1);
		}
		if (cut != 2)
			return 0; // (eax = cut - 2)
		a_73C100(a1, 0x7B82C0, 0x176A958, 0, 0xF, 0);
		x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BCB40, 0xB4, a1);
		x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BBC20, 0xB4, a1);
		ud_7B5B30(a1, 0x7B5B80, U32(MEM<uint32_t>(0x25EAD84), 0x124), 0xEC, 0, 0);
		ud_7B5B30(a1, 0x7B80B0, U32(MEM<uint32_t>(0x25EAD84), 0x128), 0x134, 1, 0);
		ud_7B5B30(a1, 0x7B81C0, U32(MEM<uint32_t>(0x25EAD84), 0x12C), 0x134, 2, 0);
		return x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BCE40, 0xB4, a1);
	}

	// 0x7B5B30 (sub_7B5B30): spawns a prim-model sequence task a2 (0x2C0 bytes, queue 0x25E9728,
	// parent a1) with sequence file a3 (+0x74), layout index (s16)a4 (+0x78), id a5 (+0x80), a6 (+0x82)
	uint32_t __cdecl ud_7B5B30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25E9728, a2, 0x2C0, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x7B5BE0 (sub_7B5BE0): prim-model sequence task 0x7B5B80, state 0 - decodes the layout
	// (+0x74 file, +0x78 index -> +0x94), scale +0x50/+0x54/+0x58 = 0x1000, position = the caster
	// entity's (+0x2C slot: entity +0x1C / +0x20 dwords) with y - 0x800, +0x88 = -0x200, first
	// draw (0x7B5C50), next state
	uint32_t __cdecl ud_7B5BE0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2C);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t w1c = U32(ent, 0x1C);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		const uint32_t w20 = U32(ent, 0x20);
		U32(node, 0x1C) = w1c;
		U32(node, 0x20) = w20;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xF800);
		U16(node, 0x88) = 0xFE00;
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_play(node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7B5D80 (sub_7B5D80): prim-model player draw callback of the sequence tasks (layout a1,
	// record a2, block a3; Psycho Blast pb_893B20 without the 0x4000 / 0x2000 header flags) - picks
	// the object's vertex frame (lerped by MAG_017_sub_701390 into the block's buffer +0x48), builds
	// its matrix (rotation 0x701310; parent-rotated position unless record flag 0x200; scale: the
	// diagonal product 0x56C220 with flag 0x100, else scale3DMatrix), fills a 0x68-byte Field_Alloc
	// render header (0x2030, 0x20F0 with alpha, colour, depth offset block +0x58, scale words 0x100)
	// and draws it with 0x7B6020 into OT base+0x44 (shift 2) at the battle packet cursor 0x1D8E054
	uint32_t __cdecl ud_7B5D80(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0 SVECTOR position (+6 pad unwritten, never read), +8 scale (3 x int32, or the 3x3
		// s16 diagonal matrix of the 0x100 path), +0x28 Mat4x3 object matrix (+0x3C translation)
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
				U16(S, 8) = U16(rec, 0x1A);
				U16(S, 0) = U16(rec, 0x18);
				U16(S, 0xA) = 0;
				U16(S, 4) = 0;
				U16(S, 0xE) = 0;
				U16(S, 2) = 0;
				U16(S, 0xC) = 0;
				U16(S, 6) = 0;
				U16(S, 0x10) = U16(rec, 0x1C);
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
		const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		U32(hdr, 0x10) = (uint32_t)(int32_t)S16(blk, 0x58);   // depth offset
		U16(hdr, 0x22) = 0x100;
		U16(hdr, 0x20) = 0x100;
		U16(hdr, 0x2A) = 0x100;
		U16(hdr, 0x28) = 0x100;
		MEM<uint32_t>(0x1D8E054) = ud_7B6020(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 ud_7B6600, FT4 ud_7B6A80,
	// a_73D770, a_73D9C0, 0x73DCA0, ud_7B7A30) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl ud_7B6020(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = ud_7B6150(ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ud_7B6370(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ud_7B6600(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = ud_7B6A80(ctx, a2, a3, cur);
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
			cur = ud_7B7530(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return ud_7B7A30(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x8C5F00 (copy of t_764670 0x764670: module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl ud_7B6150(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7B6370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7B6600(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7B6A80(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7B7530(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7B7A30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
	uint32_t __cdecl ud_7B8080(uint32_t a1)
	{
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_play(a1);)
		const uint32_t r = a_73C280(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x7B8110 (sub_7B8110): prim-model sequence task 0x7B80B0, state 0 - from its tick 4: decodes
	// the layout (+0x74 file, +0x78 index -> +0x94), scale 0x1000, position = the anchor dwords
	// 0x25DF3C0 / 0x25DF3C4, +0x88 = -0x200, first draw (0x7B5C50), next state
	uint32_t __cdecl ud_7B8110(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 4)
			return 0; // void
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t w20 = MEM<uint32_t>(0x25DF3C4);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U32(node, 0x1C) = MEM<uint32_t>(0x25DF3C0);
		U32(node, 0x20) = w20;
		U16(node, 0x88) = 0xFE00;
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_play(node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7B8170 (sub_7B8170): prim-model sequence task 0x7B80B0, state 1 - follows the anchor dwords
	// 0x25DF3C0 / 0x25DF3C4, draws (0x7B5C50); when the sequence ends: finished, next state
	uint32_t __cdecl ud_7B8170(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t w1c = MEM<uint32_t>(0x25DF3C0);
		const uint32_t w20 = MEM<uint32_t>(0x25DF3C4);
		U32(node, 0x1C) = w1c;
		U32(node, 0x20) = w20;
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_play(node);)
		const uint32_t r = a_73C280(node);
		if (r == 0)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7B8220 (sub_7B8220): prim-model sequence task 0x7B81C0, state 0 - the same code as 0x7B5BE0
	uint32_t __cdecl ud_7B8220(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2C);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t w1c = U32(ent, 0x1C);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		const uint32_t w20 = U32(ent, 0x20);
		U32(node, 0x1C) = w1c;
		U32(node, 0x20) = w20;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xF800);
		U16(node, 0x88) = 0xFE00;
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_play(node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x8C8840 (copy of s_73F220 0x73F220: module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl ud_7B8290(uint32_t a1)
	{
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_play(a1);)
		const uint32_t r = a_73C280(a1);
		if (r == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x8C8930 (copy of mb_825410 0x825410: copy of db_846A80 0x846A80: copy of cg_87E200 0x87E200: MAG_028_CURAGA_MainVisual_State0_GateAndAdvance; = Confuse c_85FBA0, copy of a_7348A0 0x7348A0; engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl ud_7B8320(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			ud_7B8E30(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8C8B10 (copy of mb_8255F0 0x8255F0: copy of db_846C60 0x846C60: copy of cg_87E3E0 0x87E3E0: sub_87E3E0; = Confuse c_85FD80, copy of b_72AD60 0x72AD60; 097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl ud_7B8500(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(0x25EB3BC);
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
	uint32_t __cdecl ud_7B8780(void)
	{
		const uint32_t arena = MEM<uint32_t>(0x25EB3BC);
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
	uint32_t __cdecl ud_7B8E30(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(0x25EB3BC, 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			ud_7BBB60();
			ud_7B91C0();
			// 30 fps layer: see mag079_ultimecia_final_death_held.inc
			FX_HELD(held_note_system(sys);)
			ud_7B9080();
			ud_7B8EC0();
			sys = U32(0x25EB3BC, 0);
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
		U16(0x25EB390, 0) = (uint16_t)(U16(0x25EB390, 0) + c14);
		U16(0x25EAF30, 0) = (uint16_t)(U16(0x25EAF30, 0) + c16);
		return done;
	}

	// 0x896E70 (copy of rt_8A4670 0x8A4670: copy of mb_825FD0 0x825FD0: copy of db_8476F0 0x8476F0: copy of cg_87ED10 0x87ED10: sub_87ED10; = Confuse c_8606B0, copy of a_735440 0x735440; engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl ud_7B8EC0(void)
	{
		uint32_t act = U32(U32(0x25EB3BC, 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(0x25EB3BC, 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					ud_7B8F20(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x896ED0 (copy of rt_8A46D0 0x8A46D0: copy of dm_8C9530 0x8C9530: copy of d_8B8520 0x8B8520: sub_8B8520): draw of a prim-model actor a1 (definition a2): header 0x68 bytes on the
	// module scratch stack (model +0x170, flags 0 / 0x30 by definition +0x3A, fade +0x1CE / colour
	// +0x16C, scale words 0x100), one copy or one per sub-position +0x19C.., module renderer 0x8C5DD0 into the frame arena 0x1D8E054
	uint32_t __cdecl ud_7B8F20(uint32_t a1, uint32_t a2)
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
			MEM<uint32_t>(0x1D8E054) = ud_7B6020(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
				MEM<uint32_t>(0x1D8E054) = ud_7B6020(h, MEM<uint32_t>(0x1D8E04C) + 0x44, 2, MEM<uint32_t>(0x1D8E054));
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
	uint32_t __cdecl ud_7B9080(void)
	{
		uint32_t act = U32(MEM<uint32_t>(0x25EB3BC), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(0x25EB3BC);
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						ud_7B9140(act);   // (the original also pushes the bone entry, unused)
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
							ud_7B9140(act);
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
	uint32_t __cdecl ud_7B9140(uint32_t a1)
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
	uint32_t __cdecl ud_7B91C0(void)
	{
		uint32_t act = U32(U32(0x25EB3BC, 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				ud_7BA490(act);
			else if (type == 1)
				ud_7B9200(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x848EF0 (copy of cg_880510 0x880510: Effect_Particle_DrawAndAdvance; = Confuse c_861EB0, copy of a_740880 0x740880 whose callee chain reaches this module's
	// particle pool 0x881700): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl ud_7B9200(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(0x25EB3BC);
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (ud_7BA340(act, bone) == 0)
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
			st = MEM<uint32_t>(0x25EB3BC);
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
	uint32_t __cdecl ud_7BA230(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(0x25EB3BC, 0);                  // actor state block
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
	uint32_t __cdecl ud_7BA340(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				ud_7BA3C0(a1, a2);
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
			ud_7BA3C0(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			ud_7BA3C0(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x828D30 (copy of db_84A0B0 0x84A0B0: copy of cg_8816D0 0x8816D0: sub_8816D0; = Confuse c_863070, copy of a_741A40 0x741A40, callee 0x881700): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl ud_7BA3C0(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return ud_7BA3F0(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x828D60 (copy of db_84A0E0 0x84A0E0: copy of cl_8770F0 0x8770F0: copy of d_8BBC10: 0x8BBC10 (sub_8BBC10)): the engine's 0x741A70 (allocates an emitter actor record of 0x6C bytes
	// from the pool [0x2742824], cursor 0x273B538) with the module's pool of 0x14 records
	uint32_t __cdecl ud_7BA3F0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t pool = MEM<uint32_t>(0x25EAF34);
		int32_t idx = MEM<int16_t>(0x25E8950);
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
			MEM<uint16_t>(0x25E8950) = (uint16_t)idx;
		else
			MEM<uint16_t>(0x25E8950) = 0;
		return slot;
	}

	// 0x8CAAA0 (copy of mb_828E00 0x828E00: copy of db_84A180 0x84A180: copy of cg_8817A0 0x8817A0: sub_8817A0; = Confuse c_863140, copy of a_737ED0 0x737ED0; engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (ud_7BB680, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl ud_7BA490(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			ud_7BB680(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(0x7BAE40, a1);
		else if (mode == 4)
			callp(0x7BAF90, a1);
		ud_7BA510(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x8BBD30 (copy of a_737F50 0x737F50; engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl ud_7BA510(uint32_t a1)
	{
		uint32_t sys = U32(0x25EB3BC, 0);
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
				ud_7BA590(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			ud_7BA590(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x8BBDB0 (copy of a_737FD0 0x737FD0; engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl ud_7BA590(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(0x25EB3AC, 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x25EB3AC, 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = ud_7BAD70(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x25EB3BC, 0);
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
		ud_7BBAD0(node, desc);
		U32(0x25EB3AC, 0) = U32(0x25EB3AC, 0) + 0x50;
		return 0; // void
	}

	// 0x8BC590 (sub_8BC590): the engine's 0x7387B0 (allocates a particle actor record of 0x2A0 bytes
	// from the pool [0x2742868], cursor 0x2742830) with the module's pool of 0x3C records
	uint32_t __cdecl ud_7BAD70(uint32_t a1, uint32_t a2)
	{
		const uint32_t pool = MEM<uint32_t>(0x25EB3B0);
		int32_t i = MEM<int16_t>(0x25EAF40);
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
			MEM<uint16_t>(0x25EAF40) = 0;
		else
			MEM<uint16_t>(0x25EAF40) = (uint16_t)i;
		return slot;
	}

	// 0x8CB5A0 (copy of mb_829900 0x829900: copy of db_84AC80 0x84AC80: copy of cg_8822A0 0x8822A0: sub_8822A0; = Confuse c_863C40, copy of b_72EF80 0x72EF80; Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl ud_7BAF90(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(0x25EB3BC, 0);            // ecx
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
				uint32_t g = U32(0x25EB3BC, 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(0x25EB3BC, 0);
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

	// 0x86ADE0 (sub_86ADE0, copy of a_742D00 0x714370; engine; Siren sub_742D00, in 095-100): particle init: copies descriptor placement
	// kind +0x11 to +0x68, builds the particle matrix +0x2C by descriptor +0x1C (identity / yaw
	// focus->anchor / yaw anchor->focus / owner matrix / yaw of battle entity dir+0x1E), places it
	// by kind (0x742EB0 / 0x742FF0 / 0x7424C0 / 0x742610) and sets its life +0x64 = desc +0x12
	uint32_t __cdecl ud_7BB680(uint32_t a1)
	{
		uint32_t dir = U32(0x25EB3BC, 0);
		uint32_t table = U32(dir, 0x224);
		int32_t di = (int32_t)S8(a1, 0x6A);
		uint32_t owner = U32(a1, 0x5C);
		uint32_t desc = U32(table + di * 4, 0); // ebp, kept across the calls
		U8(a1, 0x68) = U8(desc, 0x11);
		uint32_t m = a1 + 0x2C;
		switch ((uint32_t)(int32_t)S8(desc, 0x1C))
		{
			case 0: // 0x742D38
				x::MAG_022_sub_8DD770(m);
				break;
			case 1: // 0x742D49: yaw of (focus dir+0x1C4/0x1CC - anchor point dir+0x54/0x5C + 16*anchor)
			{
				x::MAG_022_sub_8DD770(m);
				int32_t k = shl32((int32_t)S8(a1, 0x6B), 4);
				uint32_t d = U32(0x25EB3BC, 0);
				uint32_t e = (uint32_t)k + d;
				int32_t dx = sub32(S32(d, 0x1C4), S32(e, 0x54));
				int32_t dz = sub32(S32(d, 0x1CC), S32(e, 0x5C));
				uint32_t ang = x::CartesianToGameAngle((uint32_t)dx, (uint32_t)dz);
				if (ang != 0)
					x::MAG_022_sub_8DD8A0(m, ang);
				break;
			}
			case 2: // 0x742D8B: yaw of (anchor point - focus)
			{
				x::MAG_022_sub_8DD770(m);
				int32_t k = shl32((int32_t)S8(a1, 0x6B), 4);
				uint32_t d = U32(0x25EB3BC, 0);
				int32_t dx = sub32(S32((uint32_t)k + d, 0x54), S32(d, 0x1C4));
				int32_t dz = sub32(S32((uint32_t)k + d, 0x5C), S32(d, 0x1CC));
				uint32_t ang = x::CartesianToGameAngle((uint32_t)dx, (uint32_t)dz);
				if (ang != 0)
					x::MAG_022_sub_8DD8A0(m, ang);
				break;
			}
			case 3: // 0x742DCB: owner instance matrix
				memcpy((void *)m, (void *)(owner + 0x8C), 32);
				break;
			case 4: // 0x742DDD: yaw of battle entity dir+0x1E (entity +0xE)
			{
				x::MAG_022_sub_8DD770(m);
				uint32_t d = U32(0x25EB3BC, 0);
				int32_t ent = (int32_t)S16(d, 0x1E);
				int32_t yaw = (int32_t)S16(0x1D972CE + (uint32_t)(ent * 39) * 4, 0);
				if (yaw != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)yaw);
				break;
			}
			default:
				break;
		}
		// 0x742E11
		switch ((uint32_t)(int32_t)S8(a1, 0x68))
		{
			case 0: a_742EB0(a1); break;             // 0x742E21
			case 1: ud_7BB970(a1); break;             // 0x742E38 (listing: not ported yet; part e4 ports it)
			case 2:
			case 3: a_7424C0(a1); break;             // 0x742E4F
			case 4: callp(0x7BAF90, a1); break;   // 0x742E66
			default: break;
		}
		U16(a1, 0x64) = (uint16_t)(int16_t)S8(desc, 0x12);
		return 0; // void
	}

	// 0x7BB970 (sub_7BB970; Siren a_742FF0 with a random spawn point): places particle a1 at one of
	// the actor state's points (state 0x25EB3BC: +0x1D4 / +0x1E4 / +0x204 by descriptor +0x25 = 0 / 1 /
	// 3; 2 = GetEffectSpawnPosition of the entity state+0x1E with the random bone of the 18-entry
	// table 0x1581EF0, <<16), then adds the descriptor offset (desc +0/+4/+8, 16.16) turned by the
	// particle matrix +0x2C (<<16)
	uint32_t __cdecl ud_7BB970(uint32_t a1)
	{
		// [esp+8]: +0 spawn position (SVECTOR), +8 offset SVECTOR (+0xE pad never written), +0x10 IR123
		alignas(4) uint8_t loc[0x18] = {};
		const uint32_t L = P(loc);
		const uint32_t dir = MEM<uint32_t>(0x25EB3BC);
		const uint32_t desc = U32(U32(dir, 0x224) + (uint32_t)((int32_t)S8(a1, 0x6A) * 4), 0);
		uint32_t base = 0;
		switch ((uint32_t)(int32_t)S8(desc, 0x25))
		{
			case 0: base = 0x1D4; break;
			case 1: base = 0x1E4; break;
			case 3: base = 0x204; break;
			case 2:
			{
				const uint32_t ent = 0x1D972C0 + (uint32_t)((int32_t)S16(dir, 0x1E) * 0x9C);
				const int32_t r = (int32_t)x::CrtRand();
				const int32_t bone = S16(0x1581EF0 + (uint32_t)((r % 0x12) * 2), 0);
				x::GetEffectSpawnPosition(ent, (uint32_t)bone, 0, L);
				U32(a1, 0x4C) = (uint32_t)shl32(S16(L, 0), 16);
				U32(a1, 0x50) = (uint32_t)shl32(S16(L, 2), 16);
				U32(a1, 0x54) = (uint32_t)shl32(S16(L, 4), 16);
				break;
			}
			default: break;
		}
		if (base != 0)
		{
			const uint32_t src = dir + base;
			U32(a1, 0x4C) = U32(src, 0);
			U32(a1, 0x50) = U32(src, 4);
			const uint32_t s8 = U32(src, 8);
			const uint32_t sc = U32(src, 0xC);
			U32(a1, 0x54) = s8;
			U32(a1, 0x58) = sc;
		}
		// 0x7BBA2A: cdq / and edx, 0xFFFF / add / sar 16 (16.16 -> integer, toward zero)
		U16(L, 8) = (uint16_t)(S32(desc, 0) / 65536);
		U16(L, 0xA) = (uint16_t)(S32(desc, 4) / 65536);
		U16(L, 0xC) = (uint16_t)(S32(desc, 8) / 65536);
		x::GTE_SetRotMatrix(a1 + 0x2C);
		x::GTE_LoadV0(L + 8);
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(L + 0x10);
		const int32_t ir1 = S16(L, 0x10);
		const int32_t ir2 = S16(L, 0x12);
		const int32_t ir3 = S16(L, 0x14);
		U32(a1, 0x4C) = (uint32_t)add32(S32(a1, 0x4C), shl32(ir1, 16));
		U32(a1, 0x50) = (uint32_t)add32(S32(a1, 0x50), shl32(ir2, 16));
		U32(a1, 0x54) = (uint32_t)add32(S32(a1, 0x54), shl32(ir3, 16));
		return 0; // void
	}

	// 0x82A3F0 (copy of db_84B770 0x84B770: copy of cg_882D90 0x882D90: sub_882D90; = Confuse c_864730, copy of a_743100 0x743100, callee 0x881700): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl ud_7BBAD0(uint32_t a1, uint32_t a2)
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
			ud_7BA3F0(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			ud_7BA3F0(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			ud_7BA3F0(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			ud_7BA3F0(obj, id, variant);
		}
		return 0; // void
	}

	// 0x82A480 (copy of db_84B800 0x84B800: copy of cg_882E20 0x882E20: Effect_ParticleEmitter_EmitSpriteSlots; = Confuse c_8647C0, copy of a_743190 0x743190, callee 0x881700): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via ud_7BA3F0(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl ud_7BBB60(void)
	{
		uint32_t dir = MEM<uint32_t>(0x25EB3BC);  // eax (re-read only after the calls)
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
					ud_7BA3F0(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(0x25EB3BC);
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				ud_7BA3F0(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(0x25EB3BC);
			}
		}
		return 0; // void
	}

	// 0x8CC1B0 (copy of mb_82A510 0x82A510: copy of db_84B890 0x84B890: copy of cg_882EB0 0x882EB0: sub_882EB0; = Confuse c_864850, copy of a_7395E0 0x7395E0; engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl ud_7BBBF0(uint32_t a1)
	{
		if (ud_7B8E30(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7BBC80 (sub_7BBC80): task 0x7BBC20 state 0 - counter +0xB0 = 0x32, next state
	uint32_t __cdecl ud_7BBC80(uint32_t a1)
	{
		U16(a1, 0xB0) = 0x32;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7BBCA0 (sub_7BBCA0): task 0x7BBC20 state 1 - one spark burst per tick (0x7BBCD0) for +0xB0
	// ticks, then finished, next state
	uint32_t __cdecl ud_7BBCA0(uint32_t a1)
	{
		ud_7BBCD0(a1);
		U16(a1, 0xB0) = (uint16_t)(U16(a1, 0xB0) - 1);
		if (S16(a1, 0xB0) <= 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BBCD0 (sub_7BBCD0): spark burst - puts the node at the caster's bone 0x31
	// (GetEffectSpawnPosition -> +0x1C) and starts one spark-pair task 0x7BBDF0 (0xB4 bytes, queue
	// 0x25E5100); each of its 2 sparks (task +0x6C + 8i) starts at the node position plus a random
	// offset (0, 0, 0x200..0x3FF) turned by a random Y / X rotation (0..0xFFF each, MAG_022 matrix
	// helpers) and gets a velocity (0, 0, 0x200..0x3FF) turned the same way (task +0x8C + 8i); rand()
	// order: Y, X, offset, velocity per spark
	uint32_t __cdecl ud_7BBCD0(uint32_t a1)
	{
		// [esp+0x10]: +0 SVECTOR (pad +6 never written, never read), +8 MATRIX
		alignas(4) uint8_t loc[0x28] = {};
		const uint32_t L = P(loc);
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2C);
		x::GetEffectSpawnPosition(0x1D972C0 + slot * 0x9C, 0x31, 0, node + 0x1C);
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BBDF0, 0xB4, node);
		uint32_t s = t + 0x6C;
		for (int i = 0; i < 2; i++, s += 8)
		{
			x::MAG_022_sub_8DD770(L + 8);
			const uint32_t ry = x::CrtRand() & 0xFFF;
			x::MAG_022_sub_8DD8A0(L + 8, ry);
			const uint32_t rx = x::CrtRand() & 0xFFF;
			x::sub_8DD7E0(L + 8, rx);
			U16(L, 0) = 0;
			U16(L, 2) = 0;
			U16(L, 4) = (uint16_t)((x::CrtRand() & 0x1FF) + 0x200);
			x::matrixMultiplyVector(L + 8, L, L);
			U32(s, 0) = U32(node, 0x1C);
			U32(s, 4) = U32(node, 0x20);
			U16(s, 0) = (uint16_t)(U16(s, 0) + U16(L, 0));
			U16(s, 2) = (uint16_t)(U16(s, 2) + U16(L, 2));
			U16(s, 4) = (uint16_t)(U16(s, 4) + U16(L, 4));
			U16(L, 0) = 0;
			U16(L, 2) = 0;
			U16(L, 4) = (uint16_t)((x::CrtRand() & 0x1FF) + 0x200);
			x::matrixMultiplyVector(L + 8, L, s + 0x20);
		}
		return 0; // void
	}

	// 0x7BBDF0 (sub_7BBDF0): spark-pair TASK - state table {0x7BC580 sprite set-up, 0x7BC5A0 frame
	// step, 0x7BC5F0 ret}, then moves its 2 sparks (+0x6C + 8i) by velocity / 16 (+0x8C + 8i, C
	// division) and draws each (position -> +0x1C/+0x20, 0x7BBEB0)
	uint32_t __cdecl ud_7BBDF0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7BC580;
		states[1] = 0x7BC5A0;
		states[2] = 0x7BC5F0;
		callp(states[S8(node, 0x29)], node);
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_spark(node);)
		uint32_t c = node + 0x6C;
		for (int i = 0; i < 2; i++)
		{
			const int32_t vx = S16(c, 0x20);
			c += 8;
			U16(c, -8) = (uint16_t)(U16(c, -8) + (uint16_t)(vx / 16));
			const int32_t vy = S16(c, 0x1A);
			U16(c, -6) = (uint16_t)(U16(c, -6) + (uint16_t)(vy / 16));
			const int32_t vz = S16(c, 0x1C);
			U16(c, -4) = (uint16_t)(U16(c, -4) + (uint16_t)(vz / 16));
		}
		uint32_t e = node + 0x6C;
		for (int i = 0; i < 2; i++, e += 8)
		{
			U32(node, 0x1C) = U32(e, 0);
			U32(node, 0x20) = U32(e, 4);
			ud_7BBEB0(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x8CD770 (copy of t_7672F0 0x7672F0: module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl ud_7BBEB0(uint32_t a1)
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
		const uint32_t cursor = MEM<uint32_t>(0x25DF33C);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x25DF33C) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x7BC580 (sub_7BC580): spark-pair task state 0 - sprite sequence 0x1580E54 (+0x4C), depth
	// +0x56 = -0x80, last frame +0x52 = 0xF, next state
	uint32_t __cdecl ud_7BC580(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x1580E54;
		U16(a1, 0x56) = 0xFF80;
		U16(a1, 0x52) = 0xF;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl ud_7BC5A0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x73F250 (module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl ud_7BC610(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x7BC670, 0x7BC6B0, 0x7BCAF0, 0x7BCB30 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return task_end(a1, status);
	}

	// 0x7BC670 (au_re_BdPlaySE_2): task 0x7BC610 state 0 - from tick 0x5A: counter +0xB0 = 8, sound
	// 0x1580B28 (BdPlaySE), next state
	uint32_t __cdecl ud_7BC670(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x5A)
			return 0; // void
		U16(a1, 0xB0) = 8;
		x::BdPlaySE(0x1580B28, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7BC6B0 (sub_7BC6B0): task 0x7BC610 state 1 - two debris bursts per tick (0x7BC720 mode 1)
	// at the angle +0xB2 stepped by -0x40 before each (& 0xFFF), for +0xB0 ticks; then +0xB0 = 0x34,
	// next state
	uint32_t __cdecl ud_7BC6B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t ang = (uint32_t)(uint16_t)(U16(node, 0xB2) - 0x40) & 0xFFF;
		U16(node, 0xB2) = (uint16_t)ang;
		ud_7BC720(node, 1, ang);
		ang = (uint32_t)(uint16_t)(U16(node, 0xB2) - 0x40) & 0xFFF;
		U16(node, 0xB2) = (uint16_t)ang;
		ud_7BC720(node, 1, ang);
		U16(node, 0xB0) = (uint16_t)(U16(node, 0xB0) - 1);
		if (S16(node, 0xB0) <= 0)
		{
			U16(node, 0xB0) = 0x34;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BC720 (sub_7BC720): debris burst at the caster's bone 0x31 (+0x2C slot) - mode a2 == 1: the
	// node at GetEffectSpawnPosition, the matrix = Y 0x800 then X (s16)a3 rotation (MAG_022 helpers);
	// else the bone's matrix (sub_8DE8F0) whose translation gives the node position; starts one debris
	// task 0x7BC840 (0xB4 bytes, queue 0x25E5100) whose 2 pieces (+0x6C + 8i) start at the node and
	// get the velocity ((rand & 0x7FF) - 0x400, (rand & 0x7FF) - 0x400, -0x1000 - (rand & 0x7FF))
	// turned by the matrix (+0x8C + 8i)
	uint32_t __cdecl ud_7BC720(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// [esp+0xC]: MATRIX (0x20 bytes)
		alignas(4) uint8_t loc[0x20] = {};
		const uint32_t L = P(loc);
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2C);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		const uint32_t pos = node + 0x1C;
		if ((uint16_t)a2 == 1)
		{
			x::MAG_022_sub_8DD770(L);
			x::MAG_022_sub_8DD8A0(L, 0x800);
			x::sub_8DD7E0(L, (uint32_t)(int32_t)(int16_t)a3);
			x::GetEffectSpawnPosition(ent, 0x31, 0, pos);
		}
		else
		{
			// RESIDUE (UNINIT 0x7BC782): 0x8DE8F0 writes only the translation (+0x14..+0x1F) of the matrix; its
			// rotation (+0x00..+0x11) is what earlier code left in these stack words (g_res)
			res_rotation(node, L);
			gx::sub_8DE8F0(ent, 0x31, 0, L);
			const uint16_t tx = U16(L, 0x14);
			const uint16_t ty = U16(L, 0x18);
			const uint16_t tz = U16(L, 0x1C);
			U16(node, 0x1E) = ty;
			U16(pos, 0) = tx;
			U16(node, 0x20) = tz;
		}
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BC840, 0xB4, node);
		uint32_t v = t + 0x8C;
		for (int i = 0; i < 2; i++, v += 8)
		{
			const uint32_t w0 = U32(pos, 0);
			const uint32_t w1 = U32(pos, 4);
			U32(v, -0x20) = w0;
			U32(v, -0x1C) = w1;
			U16(v, 0) = (uint16_t)((x::CrtRand() & 0x7FF) - 0x400);
			U16(v, 2) = (uint16_t)((x::CrtRand() & 0x7FF) - 0x400);
			U16(v, 4) = (uint16_t)(0xFFFFF000u - (x::CrtRand() & 0x7FF));
			x::matrixMultiplyVector(L, v, v);
		}
		return 0; // void
	}

	// 0x7BC840 (sub_7BC840): debris TASK - state table {0x7BCAA0 sprite set-up, 0x7BCAC0 frame step,
	// 0x7BCAE0 ret}; its 2 pieces (+0x6C + 8i, velocity +0x8C + 8i): vy += 0x100, velocity -= velocity
	// / 8 (each axis, C division), position += velocity / 16; each drawn at scale 0x2000 (0x7BC950)
	uint32_t __cdecl ud_7BC840(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7BCAA0;
		states[1] = 0x7BCAC0;
		states[2] = 0x7BCAE0;
		callp(states[S8(node, 0x29)], node);
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_debris(node);)
		uint32_t c = node + 0x8E;
		for (int i = 0; i < 2; i++)
		{
			U16(c, 0) = (uint16_t)(U16(c, 0) + 0x100);
			int16_t v = S16(c, -2);
			v = (int16_t)(v - (int16_t)((int32_t)v / 8));
			c += 8;
			U16(c, -0xA) = (uint16_t)v;
			v = S16(c, -8);
			v = (int16_t)(v - (int16_t)((int32_t)v / 8));
			U16(c, -8) = (uint16_t)v;
			v = S16(c, -6);
			v = (int16_t)(v - (int16_t)((int32_t)v / 8));
			U16(c, -6) = (uint16_t)v;
			U16(c, -0x2A) = (uint16_t)(U16(c, -0x2A) + (uint16_t)((int32_t)S16(c, -0xA) / 16));
			U16(c, -0x28) = (uint16_t)(U16(c, -0x28) + (uint16_t)((int32_t)S16(c, -8) / 16));
			U16(c, -0x26) = (uint16_t)(U16(c, -0x26) + (uint16_t)((int32_t)S16(c, -6) / 16));
		}
		uint32_t e = node + 0x6C;
		for (int i = 0; i < 2; i++, e += 8)
		{
			U32(node, 0x1C) = U32(e, 0);
			U32(node, 0x20) = U32(e, 4);
			ud_7BC950(node, 0x2000);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7BC950 (sub_7BC950; Siren s_744D30 drawing at the module sprite cursor 0x25DF33C): draws the
	// node's sequence sprite (a_7435E0, OT = effect OT +0x44, 2) at +0x1C/+0x1E/+0x20 through a 0xE8
	// scratch: camera rotation unpacked at +0xB8, scaled by a2 (s16, 1.0 = 0x1000) when != 0x1000,
	// rotated by the camera, position transformed by the camera; the result is loaded as GTE
	// rotation/translation. Skipped when +0x26 bit2 (hidden)
	uint32_t __cdecl ud_7BC950(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0;  // void
		const uint32_t s = x::Field_Alloc(0xE8);
		const uint32_t mat = s + 0xB8;  // Mat4x3: 3x3 s16 + translation s32[3] at +0x14 (s+0xCC)
		x::UnpackRotationMatrix(0x1D97778, mat);
		const uint16_t scale = (uint16_t)a2;
		if (scale != 0x1000)
		{
			const int32_t sc = (int16_t)scale;
			U32(s, 0xE0) = (uint32_t)sc;
			U32(s, 0xDC) = (uint32_t)sc;
			U32(s, 0xD8) = (uint32_t)sc;
			x::scale3DMatrix(mat, s + 0xD8);
		}
		const int32_t px = S16(node, 0x1C);
		const int32_t py = S16(node, 0x1E);
		const int32_t pz = S16(node, 0x20);
		const uint32_t pos = s + 0xCC;
		U32(s, 0xD0) = (uint32_t)py;
		U32(s, 0xD4) = (uint32_t)pz;
		U32(pos, 0) = (uint32_t)px;
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(mat);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(mat);
		x::GTE_LoadIRFromMatrixColumn(s + 0xBA);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(s + 0xBA);
		x::GTE_LoadIRFromMatrixColumn(s + 0xBC);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(s + 0xBC);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(pos);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(pos);
		x::GTE_SetRotMatrix(mat);
		x::GTE_SetTransVector(mat);
		const uint32_t seq = U32(node, 0x4C);
		const uint16_t frame = U16(node, 0x50);
		const uint16_t w56 = U16(node, 0x56);
		U32(s, 0) = seq;
		const uint32_t cursor = MEM<uint32_t>(0x25DF33C);
		U16(s, 4) = frame;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(s, 0x24) = 0;
		U16(s, 0xB4) = w56;
		MEM<uint32_t>(0x25DF33C) = a_7435E0(s, ot, 2, cursor);
		x::Field_Free(0xE8);
		return 0;  // void
	}

	// 0x7BCAA0 (sub_7BCAA0): debris task state 0 - sprite sequence 0x1580B30 (+0x4C), depth +0x56 =
	// -0x40, last frame +0x52 = 0x15, next state
	uint32_t __cdecl ud_7BCAA0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x1580B30;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 0x15;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl ud_7BCAC0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x7BCAF0 (sub_7BCAF0): task 0x7BC610 state 2 - one debris burst per tick off the bone matrix
	// (0x7BC720 mode 0) for +0xB0 ticks, then finished, next state
	uint32_t __cdecl ud_7BCAF0(uint32_t a1)
	{
		ud_7BC720(a1, 0, 0);
		U16(a1, 0xB0) = (uint16_t)(U16(a1, 0xB0) - 1);
		if (S16(a1, 0xB0) <= 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BCBA0 (sub_7BCBA0): task 0x7BCB40 state 0 - counter +0xB0 = 0x26, next state
	uint32_t __cdecl ud_7BCBA0(uint32_t a1)
	{
		U16(a1, 0xB0) = 0x26;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7BCBC0 (sub_7BCBC0): task 0x7BCB40 state 1 - one flake per tick (0x7BCBF0) for +0xB0 ticks,
	// then finished, next state
	uint32_t __cdecl ud_7BCBC0(uint32_t a1)
	{
		ud_7BCBF0(a1);
		U16(a1, 0xB0) = (uint16_t)(U16(a1, 0xB0) - 1);
		if (S16(a1, 0xB0) <= 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BCBF0 (sub_7BCBF0): flake - the node at the caster's bone 0xF1 (GetEffectSpawnPosition ->
	// +0x1C), one flake task 0x7BCC80 (0xB4 bytes, queue 0x25E5100) at that position plus
	// ((rand & 0x7FF) - 0x400, (rand & 0x7FF) - 0x400, (rand & 0x1FF) - 0x100) (+0x6C/+0x6E/+0x70)
	uint32_t __cdecl ud_7BCBF0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2C);
		x::GetEffectSpawnPosition(0x1D972C0 + slot * 0x9C, 0xF1, 0, node + 0x1C);
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25E5100, 0x7BCC80, 0xB4, node);
		const uint32_t w1 = U32(node, 0x20);
		const uint32_t w0 = U32(node, 0x1C);
		U32(t, 0x6C) = w0;
		U32(t, 0x70) = w1;
		U16(t, 0x6C) = (uint16_t)(U16(t, 0x6C) + (uint16_t)((x::CrtRand() & 0x7FF) - 0x400));
		U16(t, 0x6E) = (uint16_t)(U16(t, 0x6E) + (uint16_t)((x::CrtRand() & 0x7FF) - 0x400));
		U16(t, 0x70) = (uint16_t)(U16(t, 0x70) + (uint16_t)((x::CrtRand() & 0x1FF) - 0x100));
		return 0; // void
	}

	// 0x7BCC80 (sub_7BCC80): flake TASK - state table {0x7BCCF0 sprite set-up, 0x7BCD10 frame step,
	// 0x7BCD30 ret}, drawn at +0x6C/+0x70 (-> +0x1C/+0x20, 0x7BBEB0)
	uint32_t __cdecl ud_7BCC80(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x7BCCF0;
		states[1] = 0x7BCD10;
		states[2] = 0x7BCD30;
		callp(states[S8(node, 0x29)], node);
		U32(node, 0x1C) = U32(node, 0x6C);
		U32(node, 0x20) = U32(node, 0x70);
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_flake(node);)
		ud_7BBEB0(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7BCCF0 (sub_7BCCF0): flake task state 0 - sprite sequence 0x1580D78 (+0x4C), depth +0x56 =
	// -0x80, last frame +0x52 = 7, next state
	uint32_t __cdecl ud_7BCCF0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x1580D78;
		U16(a1, 0x56) = 0xFF80;
		U16(a1, 0x52) = 7;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x8CDE60 (copy of t_767430 0x767430: module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl ud_7BCD10(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x7BCDF0 (sub_7BCDF0): task 0x7BCD50 state 1 - level +0x1C += 0xA0 up to 0x800 (then finished,
	// next state), copied to word +0 of the 4 records at 0x1D98992
	uint32_t __cdecl ud_7BCDF0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xA0);
		if (S16(a1, 0x1C) >= 0x800)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x800;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t v = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 0; i < 4; i++, p += 0x2C)
			U16(p, 0) = v;
		return 0; // void
	}

	// 0x7BCEA0 (sub_7BCEA0): task 0x7BCE40 state 0 - after tick 0x5A: level +0x1C = 0x800, next state
	uint32_t __cdecl ud_7BCEA0(uint32_t a1)
	{
		if (S16(a1, 0x24) <= 0x5A)
			return 0; // void
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x800;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x8CC350 (copy of mb_82A720 0x82A720: copy of cl_8793D0 0x8793D0: copy of m_733C90: 0x733C90 (module 096 sub_733C90)): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl ud_7BCEC0(uint32_t a1)
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

	// 0x7BCF10 (sub_7BCF10): caster-model TASK - its 11-state table {0x7BD800 set-up, 0x7BD900 cut 2:
	// hide the caster + model copy, 0x7BDA60, 0x7BDCB0, 0x7BDD00, 0x7BDD40, 0x7BDD70, 0x7BDEF0, 0x7BDF90,
	// 0x7BE050, 0x7BE0B0 ret}; while +0x26 bit 3 is set and bit 2 clear: WRITES THE CASTER ENTITY's
	// position (+0x1C/+0x1E/+0x20 = model block 0x25DF348 +0x78 - +0x70 per axis, +0x54/+0x58/+0x5C
	// the sign-extended values) and draws the model copy (0x7BD020)
	uint32_t __cdecl ud_7BCF10(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[11];
		states[0] = 0x7BD800;
		states[1] = 0x7BD900;
		states[2] = 0x7BDA60;
		states[3] = 0x7BDCB0;
		states[4] = 0x7BDD00;
		states[5] = 0x7BDD40;
		states[6] = 0x7BDD70;
		states[7] = 0x7BDEF0;
		states[8] = 0x7BDF90;
		states[9] = 0x7BE050;
		states[10] = 0x7BE0B0;
		const uint32_t slot = U8(node, 0x2C);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		const int8_t st0 = S8(node, 0x29);
		callp(states[st0], node);
		const uint8_t fl = U8(node, 0x26);
		// RESIDUE: what this tick leaves in the stack words the debris burst 0x7BC720 reads (see g_res)
		g_res.kind = 0;
		if (st0 == 1 && S8(node, 0x29) == 1)
		{
			// 0x7BD900 waiting for cut 2: its frame (caller esi / edi, the gate argument 2, the return 0x7BD921)
			g_res.kind = 1;
			g_res.w[1] = 0x7BD921;
			g_res.w[2] = 2;
			g_res.w[3] = node;
			g_res.w[4] = ent;
		}
		if ((fl & 8) != 0 && (fl & 4) == 0)
		{
			// 0x7BD020's frame: its first call argument (the drawn entity +0x60), the saved edi (this node), esi
			// (the caster), ebp (the queue the game's executor runs) and ebx (the executor's count of the live tasks
			// it ran before this one in the queue)
			uint32_t live = 0;
			for (uint32_t q = MEM<uint32_t>(0x25E5100); q != 0 && q != node; q = U32(q, 4))
				live++;
			g_res.kind = 2;
			g_res.w[0] = 0x1D972C0 + (uint32_t)((int32_t)S16(0x25DF348, 0xA8) * 0x9C) + 0x60;
			g_res.w[1] = node;
			g_res.w[2] = ent;
			g_res.w[3] = 0x25E5100;
			g_res.w[4] = live;
			const uint32_t x0 = MEM<uint32_t>(0x25DF3C0) - MEM<uint32_t>(0x25DF3B8);
			U16(ent, 0x1C) = (uint16_t)x0;
			S32(ent, 0x54) = (int32_t)(int16_t)x0;
			const uint16_t y0 = (uint16_t)(MEM<uint16_t>(0x25DF3C2) - MEM<uint16_t>(0x25DF3BA));
			U16(ent, 0x1E) = y0;
			S32(ent, 0x58) = (int32_t)(int16_t)y0;
			const uint32_t z0 = MEM<uint32_t>(0x25DF3C4) - MEM<uint32_t>(0x25DF3BC);
			U16(ent, 0x20) = (uint16_t)z0;
			S32(ent, 0x5C) = (int32_t)(int16_t)z0;
			// 30 fps layer: see mag079_ultimecia_final_death_held.inc
			FX_HELD(held_note_model(node);)
			ud_7BD020(0x25DF348);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		g_res.node = node;
		g_res.stamp = U16(node, 0x24);
		return task_end(node, status);
	}

	// 0x7BD020 (sub_7BD020): draws the model copy of block a1 (0x25DF348): the entity (s16)+0xA8's
	// matrix +0x40 is copied to +0x20, its bone matrices rebuilt from its pose (0x508C90), the copy
	// turned by the camera (columns -> +0x40, translation -> +0x54); both model parts (entity +0x64,
	// and [+0x78]+4 when present) drawn by 0x7BD110 into OT base+0x44 (shift 4); the bone matrices
	// are rebuilt again after
	uint32_t __cdecl ud_7BD020(uint32_t a1)
	{
		const uint32_t blk = a1;
		const int32_t idx = S16(blk, 0xA8);
		const uint32_t ent = 0x1D972C0 + (uint32_t)(idx * 0x9C);
		memcpy((void *)(blk + 0x20), (const void *)(ent + 0x40), 0x20); // rep movsd
		const uint32_t pose = ent + 0x60;
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x20);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x40);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x22);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x42);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x24);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x44);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(blk + 0x34);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(blk + 0x54);
		ud_7BD110(U32(ent, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, blk);
		const uint32_t part2 = U32(ent, 0x78);
		if (part2 != 0)
			ud_7BD110(U32(part2, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, blk);
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		return 0; // void
	}

	// 0x7BD110 (sub_7BD110; Raldo rt_8A95F0 with the block 4 bytes lower, 16-byte vertex records and a
	// depth bias): morph renderer of a battle model (a1 = geometry {+0 bone matrices - 0x10, +4 object
	// table}, a2 = OT, a3 = depth shift, a4 = block 0x25DF348 {+0 rotation, +0x40 camera matrix, +0x60
	// -> {+4 vertex scratch}, +0x64 entity, +0x68 packet cursor pointer, +0x70 pivot, +0x98 / +0x9A /
	// +0x9C angles, +0xA0 morph, +0xA2..+0xA6 radii, +0xD0 depth bias}): per visible object (entity +0x7C
	// mask) every vertex is put in world space by its bone (light matrix + background vector), taken
	// relative to the pivot, and morphed between itself (weight 0x1000 - morph) and its direction
	// scaled by the radii (weight morph) through the GTE interpolation (0x45E0B0 / 0x45E9D0 /
	// 0x45EBF0), turned by the rotation (colour matrix, ColorV0) and put back at the pivot (16-byte
	// scratch records {x, y, z, pad, sxy, sz}); projected (RTPS), then back-face-culled textured
	// triangles (POLY_FT3, 7 words) and quads (POLY_FT4, 9 words) tinted with the entity colour
	// (+0x28), OT key + bias clamped at 0, inserted with SSIGPU_InsertPrimDepthKeys
	uint32_t __cdecl ud_7BD110(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t blk = a4;
		const uint32_t ent = U32(blk, 0x64);
		const uint32_t verts = U32(U32(blk, 0x60), 4);  // vertex scratch (ebx)
		uint32_t cursor = U32(U32(blk, 0x68), 0);       // packet cursor (local +0x1C)
		const uint32_t bones = U32(a1, 0) + 0x10;       // local +0x30
		uint32_t tab = U32(a1, 4);                      // object table {count, offsets[count]}
		const int32_t count = S32(tab, 0);              // local +0x2C
		const uint32_t w = x::Field_Alloc(0xA8);        // scratch block (ebp)
		tab += 4;
		U16(w, 0x9E) = U16(blk, 0xA0);                  // morph weight
		U16(w, 0x9C) = (uint16_t)(0x1000 - U16(blk, 0xA0));
		U32(w, 0x5C) = U32(blk, 0x70);                  // pivot x, y
		U32(w, 0x60) = U32(blk, 0x74);                  // pivot z (+ pad)
		U16(w, 0xA4) = U16(blk, 0xA6);                  // radii
		U16(w, 0xA0) = U16(blk, 0xA2);
		U16(w, 0xA6) = U16(blk, 0xD0);                  // depth bias
		const uint32_t rgb = U32(ent, 0x28) & 0xFFFFFF;
		U16(w, 0xA2) = U16(blk, 0xA4);
		U32(w, 0x94) = rgb | 0x2C000000;                // POLY_FT4 code + colour
		memcpy((void *)w, (const void *)blk, 0x20);     // rep movsd: the rotation
		U32(w, 0x90) = rgb | 0x24000000;                // POLY_FT3 code + colour
		U32(w, 0x98) = U32(ent, 0x7C);                  // visible objects
		if (U16(blk, 0x98) != 0)
			x::sub_8DD7E0(w, (uint32_t)(int32_t)S16(blk, 0x98));
		if (U16(blk, 0x9C) != 0)
			x::sub_8DD960(w, (uint32_t)(int32_t)S16(blk, 0x9C));
		if (U16(blk, 0x9A) != 0)
			x::MAG_022_sub_8DD8A0(w, (uint32_t)(int32_t)S16(blk, 0x9A));
		gx::sub_45DEA0(w);
		x::GTE_SetRotMatrix(blk + 0x40);
		x::GTE_SetTransVector(blk + 0x40);
		for (int32_t i = 0; i < count; i++)
		{
			const uint32_t obj = U32(a1, 4) + U32(tab, 0);
			tab += 4;
			if (((U32(w, 0x98) >> (i & 31)) & 1) == 0)
				continue;
			// pass 1: world vertex, relative to the pivot, morphed, turned, back
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
					wv += 0x10;
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
					vp += 0x10;
					zp += 0x10;
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
				const uint32_t s2 = U32(verts + i2 * 16, 8);
				const uint32_t s1 = U32(verts + i1 * 16, 8);
				const uint32_t s0 = U32(verts + i0 * 16, 8);
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
				const int32_t z1 = S32(verts + U32(w, 0x84) * 16, 0xC);
				const int32_t z0 = S32(verts + U32(w, 0x80) * 16, 0xC);
				const int32_t z2 = S32(verts + U32(w, 0x88) * 16, 0xC);
				const int32_t sum = add32(add32(z1, z0), z2);
				// sum / 3 (magic 0x55555556: high dword + its sign bit)
				int32_t hq = (int32_t)(((int64_t)sum * 0x55555556LL) >> 32);
				hq = add32(hq, (int32_t)((uint32_t)hq >> 31));
				int32_t key = add32(hq >> (a3 & 31), (int32_t)S16(w, 0xA6));
				U32(w, 0x78) = (uint32_t)key;
				if (key < 0)
					U32(w, 0x78) = 0;
				x::SSIGPU_InsertPrimDepthKeys(a2 + U32(w, 0x78) * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, 0);
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
					const uint32_t s2 = U32(verts + i2 * 16, 8);
					const uint32_t s1 = U32(verts + i1 * 16, 8);
					const uint32_t s0 = U32(verts + i0 * 16, 8);
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
					U32(pk, 0x18) = U32(w, 0x2C);
					U32(pk, 0) = 0x9000000;
					const uint32_t v3 = verts + i3 * 16;
					const uint32_t s3 = U32(v3, 8);
					U32(pk, 0x14) = U32(f, 8);
					U32(w, 0x30) = s3;
					U32(pk, 0x20) = s3;
					U32(pk, 0xC) = U32(f, 4);
					U16(pk, 0x1C) = U16(f, 0xC);
					U16(pk, 0x24) = U16(f, 0xE);
					U32(pk, 4) = U32(w, 0x94);
					if (U8(f, 0xB) & 2)
						U8(pk, 7) |= 2;
					const int32_t z3 = S32(v3, 0xC);
					const int32_t z2 = S32(verts + U32(w, 0x88) * 16, 0xC);
					const int32_t z1 = S32(verts + U32(w, 0x84) * 16, 0xC);
					const int32_t z0 = S32(verts + U32(w, 0x80) * 16, 0xC);
					const int32_t sum = add32(add32(add32(z0, z3), z1), z2);
					int32_t key = add32(sum >> ((a3 + 2) & 31), (int32_t)S16(w, 0xA6));
					U32(w, 0x78) = (uint32_t)key;
					if (key < 0)
						U32(w, 0x78) = 0;
					x::SSIGPU_InsertPrimDepthKeys(a2 + U32(w, 0x78) * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, (uint32_t)z3);
					pk += 0x28;
				}
			}
			cursor = pk;
		}
		U32(U32(blk, 0x68), 0) = cursor;
		x::Field_Free(0xA8);
		return 0; // void
	}

	// 0x7BD800 (sub_7BD800): caster-model task state 0 - chain transformation 0x11 of the caster
	// (0x505C00), model block set-up (0x7BD890: block 0x25DF348, 0x25DF508, 0x25EA350, the caster, its
	// slot), keeps the caster's position words in 0x25DF404 / 0x25DF408, starts the actor system
	// 0x7B82C0 (0x7B5AE0: data 0x1762E40, mode 0xF), sound 0x1580B24, next state
	uint32_t __cdecl ud_7BD800(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2C);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		gx::QueueChainTransformation(ent, 0x11);
		ud_7BD890(0x25DF348, 0x25DF508, 0x25EA350, ent, (uint32_t)(uint16_t)U8(node, 0x2C)); // movzx dx: the callee reads 16 bits
		const uint32_t w1c = U32(ent, 0x1C);
		const uint32_t w20 = U32(ent, 0x20);
		MEM<uint32_t>(0x25DF404) = w1c;
		MEM<uint32_t>(0x25DF408) = w20;
		a_73C100(node, 0x7B82C0, 0x1762E40, 0, 0xF, 0);
		x::BdPlaySE(0x1580B24, 0, 0x80);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7BD890 (sub_7BD890): model block set-up - a1 (0xD4 bytes) cleared; +0x64 = entity a4, +0x68 /
	// +0x6C = the battle packet cursor's address 0x1D8E054, +0xA8 = (u16)a5 entity index, +0x60 = the
	// render context a3 (+4 = a2, depth offset words +0x14..+0x1A = 0, 0, 0x140, 0, colour bytes
	// +0x1C..+0x1E = 0x80, +0x20 = -1, +0x24 = 0)
	uint32_t __cdecl ud_7BD890(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		x::MAG_007_sub_8DCC00(a1, 0xD4);
		U32(a1, 0x64) = a4;
		U32(a1, 0x68) = 0x1D8E054;
		U32(a1, 0x6C) = 0x1D8E054;
		U16(a1, 0xA8) = (uint16_t)a5;
		U32(a3, 4) = a2;
		U32(a1, 0x60) = a3;
		U16(a3, 0x14) = 0;
		U16(a3, 0x16) = 0;
		U16(a3, 0x18) = 0x140;
		U16(a3, 0x1A) = 0;
		U8(a3, 0x1C) = 0x80;
		U8(a3, 0x1D) = 0x80;
		U8(a3, 0x1E) = 0x80;
		U32(a3, 0x20) = 0xFFFFFFFFu;
		U16(a3, 0x24) = 0;
		return 0; // void
	}

	// 0x7BD900 (sub_7BD900): caster-model task state 1 - once cut >= 2: model drawn (+0x26 |= 8), the
	// caster entity hidden (flag byte |= 4), block words +0x92 = -0x3000, +0x94 = -0x2000, +0xC6 = 0x200,
	// the bone-0xF1 pivot (0x7BD9B0 -> +0x70), scale +0xA2/+0xA4/+0xA6 = 0x1F6, block matrix reset
	// (0x8DD770), anchor +0x78/+0x7C = the caster's position words with y + pivot y, +0xB0 = 0x10,
	// next state
	uint32_t __cdecl ud_7BD900(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t slot = U8(node, 0x2C);
		const uint32_t ent = 0x1D972C0 + slot * 0x9C;
		if (a_73B7E0(2) == 0)
			return 0; // void
		U8(node, 0x26) |= 8;
		U8(ent, 0) |= 4;
		MEM<uint16_t>(0x25DF3DA) = 0xD000;
		MEM<uint16_t>(0x25DF3DC) = 0xE000;
		MEM<uint16_t>(0x25DF40E) = 0x200;
		ud_7BD9B0(node, 0x25DF348);
		MEM<uint16_t>(0x25DF3EC) = 0x1F6;
		MEM<uint16_t>(0x25DF3EE) = 0x1F6;
		MEM<uint16_t>(0x25DF3EA) = 0x1F6;
		x::MAG_022_sub_8DD770(0x25DF348);
		const uint32_t w20 = U32(ent, 0x20);
		const uint32_t w1c = U32(ent, 0x1C);
		const uint16_t py = MEM<uint16_t>(0x25DF3BA);
		MEM<uint32_t>(0x25DF3C4) = w20;
		MEM<uint32_t>(0x25DF3C0) = w1c;
		MEM<uint16_t>(0x25DF3C2) = (uint16_t)(MEM<uint16_t>(0x25DF3C2) + py);
		U16(node, 0xB0) = 0x10;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7BD9B0 (sub_7BD9B0; Raldo rt_8AABA0 with the pivot at +0x70): a2+0x70 = the spawn point (bone
	// 0xF1) of the entity +0x2C of a1, taken with the entity at the origin and its matrix reset
	// (0x8DD770), relative to the entity; position and matrix put back
	uint32_t __cdecl ud_7BD9B0(uint32_t a1, uint32_t a2)
	{
		const uint32_t e = 0x1D972C0 + (uint32_t)U8(a1, 0x2C) * 0x9C;
		uint32_t mat[8];
		const uint32_t xy = U32(e, 0x1C);
		memcpy(mat, (const void *)(e + 0x40), 0x20);    // rep movsd
		const uint32_t z = U32(e, 0x20);
		U16(e, 0x1C) = 0;
		U16(e, 0x1E) = 0;
		U16(e, 0x20) = 0;
		x::MAG_022_sub_8DD770(e + 0x40);
		const uint32_t blk = a2;
		x::GetEffectSpawnPosition(e, 0xF1, 0, blk + 0x70);
		const uint16_t ex = U16(e, 0x1C), ey = U16(e, 0x1E), ez = U16(e, 0x20);
		U16(blk, 0x70) = (uint16_t)(U16(blk, 0x70) - ex);
		U16(blk, 0x72) = (uint16_t)(U16(blk, 0x72) - ey);
		U16(blk, 0x74) = (uint16_t)(U16(blk, 0x74) - ez);
		memcpy((void *)(e + 0x40), mat, 0x20);          // rep movsd
		U32(e, 0x1C) = xy;
		U32(e, 0x20) = z;
		return 0; // void
	}

	// 0x7BDA60 (sub_7BDA60): caster-model task state 2 - model scale step (0x7BDBE0, mode 1 for the
	// last 0xB ticks of +0xB0), pivot update (0x7BD9B0); when +0xB0 runs out: the prim-model sequence
	// task 0x7BDAD0 (0x7B5B30: file of the load context +0x130, layout 0x218, id 3), next state
	uint32_t __cdecl ud_7BDA60(uint32_t a1)
	{
		const uint32_t node = a1;
		ud_7BDBE0(node, S16(node, 0xB0) < 0xC ? 1u : 0u);
		ud_7BD9B0(node, 0x25DF348);
		U16(node, 0xB0) = (uint16_t)(U16(node, 0xB0) - 1);
		if (S16(node, 0xB0) <= 0)
		{
			ud_7B5B30(node, 0x7BDAD0, U32(MEM<uint32_t>(0x25EAD84), 0x130), 0x218, 3, 0);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BDB30 (sub_7BDB30): prim-model sequence task 0x7BDAD0, state 0 - decodes the layout (+0x74
	// file, +0x78 index -> +0x94), scale 0x1000, position = the anchor dwords 0x25DF3C0 / 0x25DF3C4,
	// +0x88 = -0x200, first draw (0x7B5C50), next state
	uint32_t __cdecl ud_7BDB30(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t w20 = MEM<uint32_t>(0x25DF3C4);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U32(node, 0x1C) = MEM<uint32_t>(0x25DF3C0);
		U32(node, 0x20) = w20;
		U16(node, 0x88) = 0xFE00;
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_play(node);)
		a_73C280(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7BDB90 (sub_7BDB90): prim-model sequence task 0x7BDAD0, state 1 - the same code as 0x7B8170
	uint32_t __cdecl ud_7BDB90(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t w1c = MEM<uint32_t>(0x25DF3C0);
		const uint32_t w20 = MEM<uint32_t>(0x25DF3C4);
		U32(node, 0x1C) = w1c;
		U32(node, 0x20) = w20;
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(held_note_play(node);)
		const uint32_t r = a_73C280(node);
		if (r == 0)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BDBE0 (sub_7BDBE0): model block motion step (block 0x25DF348) - velocity y +0x92 += +0xC6;
	// a2 == 1: every velocity word (+0x90, +0x92, +0x94) loses 1/16 (C division); anchor +0x78 / +0x7A /
	// +0x7C += velocity / 16; angle +0x98 = (+0x98 - 0x80) & 0xFFF
	uint32_t __cdecl ud_7BDBE0(uint32_t a1, uint32_t a2)
	{
		(void)a1;
		int16_t vy = (int16_t)(MEM<uint16_t>(0x25DF3DA) + MEM<uint16_t>(0x25DF40E));
		MEM<uint16_t>(0x25DF3DA) = (uint16_t)vy;
		if ((uint16_t)a2 == 1)
		{
			int16_t v = (int16_t)MEM<uint16_t>(0x25DF3D8);
			v = (int16_t)(v + (int16_t)(-((int32_t)v / 16)));
			MEM<uint16_t>(0x25DF3D8) = (uint16_t)v;
			vy = (int16_t)(vy + (int16_t)(-((int32_t)vy / 16)));
			MEM<uint16_t>(0x25DF3DA) = (uint16_t)vy;
			v = (int16_t)MEM<uint16_t>(0x25DF3DC);
			v = (int16_t)(v + (int16_t)(-((int32_t)v / 16)));
			MEM<uint16_t>(0x25DF3DC) = (uint16_t)v;
		}
		MEM<uint16_t>(0x25DF3C0) = (uint16_t)(MEM<uint16_t>(0x25DF3C0) + (uint16_t)((int32_t)(int16_t)MEM<uint16_t>(0x25DF3D8) / 16));
		MEM<uint16_t>(0x25DF3C2) = (uint16_t)(MEM<uint16_t>(0x25DF3C2) + (uint16_t)((int32_t)vy / 16));
		MEM<uint16_t>(0x25DF3C4) = (uint16_t)(MEM<uint16_t>(0x25DF3C4) + (uint16_t)((int32_t)(int16_t)MEM<uint16_t>(0x25DF3DC) / 16));
		MEM<uint16_t>(0x25DF3E0) = (uint16_t)((MEM<uint16_t>(0x25DF3E0) + 0xFF80) & 0xFFF);
		return 0; // void
	}

	// 0x7BDCB0 (sub_7BDCB0): caster-model task state 3 - motion step (0x7BDBE0 mode 1), pivot update
	// (0x7BD9B0), morph +0xA0 += 0x80 up to 0x1000 (then the y acceleration +0xC6 = -0x200, next state)
	uint32_t __cdecl ud_7BDCB0(uint32_t a1)
	{
		const uint32_t node = a1;
		ud_7BDBE0(node, 1);
		ud_7BD9B0(node, 0x25DF348);
		const uint16_t m = (uint16_t)(MEM<uint16_t>(0x25DF3E8) + 0x80);
		MEM<uint16_t>(0x25DF3E8) = m;
		if ((int16_t)m >= 0x1000)
		{
			MEM<uint16_t>(0x25DF3E8) = 0x1000;
			MEM<uint16_t>(0x25DF40E) = 0xFE00;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BDD00 (sub_7BDD00): caster-model task state 4 - motion step, pivot update; once the y
	// velocity +0x92 is <= -0x800: y acceleration +0xC6 = 0x100, next state
	uint32_t __cdecl ud_7BDD00(uint32_t a1)
	{
		const uint32_t node = a1;
		ud_7BDBE0(node, 1);
		ud_7BD9B0(node, 0x25DF348);
		if ((int16_t)MEM<uint16_t>(0x25DF3DA) <= -0x800)
		{
			MEM<uint16_t>(0x25DF40E) = 0x100;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7BDD40 (sub_7BDD40): caster-model task state 5 - motion step, pivot update; next state once
	// the y velocity +0x92 is >= 0
	uint32_t __cdecl ud_7BDD40(uint32_t a1)
	{
		const uint32_t node = a1;
		ud_7BDBE0(node, 1);
		ud_7BD9B0(node, 0x25DF348);
		if ((int16_t)MEM<uint16_t>(0x25DF3DA) >= 0)
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7BDD70 (sub_7BDD70): caster-model task state 6 - pivot update, the actor system 0x7B82C0
	// (0x7B5AE0: data 0x176C45C, mode 0xF), block scale dwords +0xAC/+0xB0/+0xB4 = 0x800 and the
	// TARGET entity's (+0x2D slot) scale pointer +0x30 = 0x25DF3F4, target colour fade (0x7BDDF0) at
	// level +0xAC, next state
	uint32_t __cdecl ud_7BDD70(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
		ud_7BD9B0(node, 0x25DF348);
		a_73C100(node, 0x7B82C0, 0x176C45C, 0, 0xF, 0);
		const uint32_t lvl = (uint32_t)U16(node, 0xAC); // mov dx: the callee reads 16 bits
		MEM<uint32_t>(0x25DF3FC) = 0x800;
		MEM<uint32_t>(0x25DF3F8) = 0x800;
		MEM<uint32_t>(0x25DF3F4) = 0x800;
		U32(ent, 0x30) = 0x25DF3F4;
		a_733950(ent, lvl, 0);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7BDEF0 (sub_7BDEF0): caster-model task state 7 - level +0xAC = 0x1000; WRITES THE TARGET
	// ENTITY's (+0x2D slot) position: +0x1C/+0x20 dwords = the anchor 0x25DF3C0 / 0x25DF3C4, then
	// x + 0x80, y - 0x20, z - 0x150, +0x54/+0x58/+0x5C = the sign-extended words; its flag word: bit 2
	// cleared, bit 11 set; colour fade (0x7BDDF0) at 0x1000, sound 0x1580B2C, next state
	uint32_t __cdecl ud_7BDEF0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t ax0 = MEM<uint32_t>(0x25DF3C0);
		U16(node, 0xAC) = 0x1000;
		const uint32_t ent = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
		const uint32_t az0 = MEM<uint32_t>(0x25DF3C4);
		U32(ent, 0x1C) = ax0;
		U32(ent, 0x20) = az0;
		U16(ent, 0x1C) = (uint16_t)(U16(ent, 0x1C) + 0x80);
		U16(ent, 0x1E) = (uint16_t)(U16(ent, 0x1E) - 0x20);
		U16(ent, 0x20) = (uint16_t)(U16(ent, 0x20) + 0xFEB0);
		S32(ent, 0x54) = S16(ent, 0x1C);
		S32(ent, 0x58) = S16(ent, 0x1E);
		S32(ent, 0x5C) = S16(ent, 0x20);
		U16(ent, 0) = (uint16_t)((U16(ent, 0) & 0xFFFB) | 0x800);
		a_733950(ent, 0x1000, 0);
		x::BdPlaySE(0x1580B2C, 0, 0x80);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7BDF90 (sub_7BDF90): caster-model task state 8 - caster colour fade (0x7BDDF0) at the level
	// +0xAE (+= 0x200, up to 0x1000), block scale y/z words = x (+0xA2 -> +0xA4/+0xA6), scale +0xB4 +=
	// 0x100 (+0xAC/+0xB0 follow); at 0x1000: hidden (+0x26 |= 4), the target's scale pointer +0x30
	// cleared, next state
	uint32_t __cdecl ud_7BDF90(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0xAE) = (uint16_t)(U16(node, 0xAE) + 0x200);
		const uint32_t tgt = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
		const uint32_t cst = 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C;
		if (S16(node, 0xAE) >= 0x1000)
			U16(node, 0xAE) = 0x1000;
		a_733950(cst, (uint32_t)U16(node, 0xAE), 0); // mov ax: the callee reads 16 bits
		const uint16_t sx = MEM<uint16_t>(0x25DF3EA);
		MEM<uint16_t>(0x25DF3EE) = sx;
		MEM<uint16_t>(0x25DF3EC) = sx;
		int32_t s = (int32_t)(MEM<uint32_t>(0x25DF3FC) + 0x100);
		MEM<uint32_t>(0x25DF3FC) = (uint32_t)s;
		if (s >= 0x1000)
		{
			U8(node, 0x26) |= 4;
			s = 0x1000;
			MEM<uint32_t>(0x25DF3FC) = 0x1000;
			U32(tgt, 0x30) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		MEM<uint32_t>(0x25DF3F8) = (uint32_t)s;
		MEM<uint32_t>(0x25DF3F4) = (uint32_t)s;
		return 0; // void
	}

	// 0x7BE050 (sub_7BE050): caster-model task state 9 - level +0xAC -= 0x200 down to 0 (then finished,
	// next state), the target's colour fade (0x7BDDF0) at that level
	uint32_t __cdecl ud_7BE050(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0xAC) = (uint16_t)(U16(node, 0xAC) + 0xFE00);
		const uint32_t tgt = 0x1D972C0 + (uint32_t)U8(node, 0x2D) * 0x9C;
		if (S16(node, 0xAC) <= 0)
		{
			U8(node, 0x26) |= 1;
			U16(node, 0xAC) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		a_733950(tgt, (uint32_t)U16(node, 0xAC), 0); // mov ax: the callee reads 16 bits
		return 0; // void
	}

	// 0x8CE250 (copy of t_768370 0x768370: module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl ud_7BE0C0(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1580B20);
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

	// 0x7BE1A0 (MAG_079_sub_7BE1A0): master state 6 - once no task ran this tick (+0x5E == 0): load
	// flag 0x25DF340 = 0, loads file 0x191 (battle/MAG078_B.9M0) to the magic buffer [0x25DF338] with the
	// callback 0x7BE1E0, next state
	uint32_t __cdecl ud_7BE1A0(uint32_t a1)
	{
		if (U16(a1, 0x5E) != 0)
			return 0; // void
		MEM<uint8_t>(0x25DF340) = 0;
		x::pre_LoadBattleFile(0x191, MEM<uint32_t>(0x25DF338), 0, 0x7BE1E0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7BE1E0 (MAG_079_sub_7BE1E0): load callback of 0x7BE1A0 - load flag 0x25DF340 = 1
	uint32_t __cdecl ud_7BE1E0(void)
	{
		MEM<uint8_t>(0x25DF340) = 1;
		return 0; // void
	}

	// 0x7BE1F0 (MAG_079_sub_7BE1F0): master state 7 - once loaded: flag = 0, hands the loaded file to
	// 0x501A20 (with the flag 0x25DF340 as its done byte), next state
	uint32_t __cdecl ud_7BE1F0(uint32_t a1)
	{
		if (MEM<uint8_t>(0x25DF340) != 1)
			return 0; // void
		MEM<uint8_t>(0x25DF340) = 0;
		gx::BS_R0win_QueueVictoryFanfare(MEM<uint32_t>(0x25DF338), 0x25DF340);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7BE220 (MAG_079_sub_7BE220): master state 8 - once 0x501A20 is done (flag 0x25DF340 != 0):
	// music channel volume command (0x501B10(0, 0x7F)), next state
	uint32_t __cdecl ud_7BE220(uint32_t a1)
	{
		if (MEM<uint8_t>(0x25DF340) == 0)
			return 0; // void
		gx::BS_Music_SetChannelVolumeCommand(0, 0x7F);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// ---- generated (gendesc.py): the module's functions served by engine ports ----
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort ENGINE_PORTS[] = {
		{ 0x7B4F20, (void *)a_73A0D0, "079 MAG_079_sub_7B4F20" },
		{ 0x7B4F30, (void *)a_73A0D0, "079 MAG_079_sub_7B4F30" },
		{ 0x7B4FC0, (void *)a_73A170, "079 MAG_079_sub_7B4FC0" },
		{ 0x7B4FF0, (void *)a_73A1A0, "079 MAG_079_sub_7B4FF0" },
		{ 0x7B51C0, (void *)a_73A380, "079 MAG_079_sub_7B51C0" },
		{ 0x7B5220, (void *)a_73A3E0, "079 sub_7B5220" },
		{ 0x7B53C0, (void *)a_73A580, "079 sub_7B53C0" },
		{ 0x7B53F0, (void *)a_73A5B0, "079 sub_7B53F0" },
		{ 0x7B5420, (void *)a_73A5E0, "079 sub_7B5420" },
		{ 0x7B5430, (void *)a_73A5E0, "079 sub_7B5430" },
		{ 0x7B5440, (void *)a_73A5E0, "079 sub_7B5440" },
		{ 0x7B5450, (void *)a_73A5E0, "079 sub_7B5450" },
		{ 0x7B5460, (void *)a_73A620, "079 sub_7B5460" },
		{ 0x7B54A0, (void *)a_73A660, "079 sub_7B54A0" },
		{ 0x7B5510, (void *)a_73A6D0, "079 nullsub_1377" },
		{ 0x7B56E0, (void *)a_73B640, "079 sub_7B56E0" },
		{ 0x7B5730, (void *)a_73B7E0, "079 sub_7B5730" },
		{ 0x7B5790, (void *)a_73A930, "079 sub_7B5790" },
		{ 0x7B57B0, (void *)a_73A950, "079 sub_7B57B0" },
		{ 0x7B5830, (void *)a_73BB60, "079 sub_7B5830" },
		{ 0x7B58A0, (void *)a_73B8A0, "079 sub_7B58A0" },
		{ 0x7B58C0, (void *)a_73B8C0, "079 sub_7B58C0" },
		{ 0x7B5970, (void *)a_7475A0, "079 sub_7B5970" },
		{ 0x7B5990, (void *)a_73A6D0, "079 nullsub_1378" },
		{ 0x7B59A0, (void *)a_73A6D0, "079 nullsub_1379" },
		{ 0x7B5AE0, (void *)a_73C100, "079 sub_7B5AE0" },
		{ 0x7B5B80, (void *)a_73A380, "079 sub_7B5B80" },
		{ 0x7B5C50, (void *)a_73C280, "079 sub_7B5C50" },
		{ 0x7B7000, (void *)a_73D770, "079 sub_7B7000" },
		{ 0x7B7250, (void *)a_73D9C0, "079 sub_7B7250" },
		{ 0x7B80A0, (void *)a_73A6D0, "079 nullsub_1380" },
		{ 0x7B80B0, (void *)a_73A380, "079 sub_7B80B0" },
		{ 0x7B81B0, (void *)a_73A6D0, "079 nullsub_1381" },
		{ 0x7B81C0, (void *)a_73A380, "079 sub_7B81C0" },
		{ 0x7B82B0, (void *)a_73A6D0, "079 nullsub_1382" },
		{ 0x7B82C0, (void *)a_73A380, "079 sub_7B82C0" },
		{ 0x7B8350, (void *)a_73F990, "079 sub_7B8350" },
		{ 0x7B85E0, (void *)a_73FC20, "079 sub_7B85E0" },
		{ 0x7B8850, (void *)a_73FE90, "079 sub_7B8850" },
		{ 0x7B8970, (void *)a_73FFB0, "079 sub_7B8970" },
		{ 0x7B8BD0, (void *)a_740210, "079 sub_7B8BD0" },
		{ 0x7B9290, (void *)a_740910, "079 sub_7B9290" },
		{ 0x7B98F0, (void *)a_740F70, "079 sub_7B98F0" },
		{ 0x7B9940, (void *)a_740FC0, "079 sub_7B9940" },
		{ 0x7B99D0, (void *)a_741050, "079 sub_7B99D0" },
		{ 0x7B9AA0, (void *)a_741120, "079 sub_7B9AA0" },
		{ 0x7BA4E0, (void *)a_741B60, "079 sub_7BA4E0" },
		{ 0x7BAC10, (void *)a_742290, "079 au_re__rand_26" },
		{ 0x7BAC40, (void *)a_7422C0, "079 sub_7BAC40" },
		{ 0x7BAC80, (void *)a_742300, "079 sub_7BAC80" },
		{ 0x7BACD0, (void *)a_742350, "079 sub_7BACD0" },
		{ 0x7BAD20, (void *)a_7423A0, "079 au_re__rand_26_0" },
		{ 0x7BAE30, (void *)a_7424B0, "079 sub_7BAE30" },
		{ 0x7BAE40, (void *)a_7424C0, "079 sub_7BAE40" },
		{ 0x7BB620, (void *)a_742CA0, "079 sub_7BB620" },
		{ 0x7BB650, (void *)a_742CD0, "079 sub_7BB650" },
		{ 0x7BB830, (void *)a_742EB0, "079 sub_7BB830" },
		{ 0x7BBC10, (void *)a_73A6D0, "079 nullsub_1383" },
		{ 0x7BBC20, (void *)a_73A380, "079 sub_7BBC20" },
		{ 0x7BBFD0, (void *)a_7435E0, "079 InitEffectSequenceFromData_c10" },
		{ 0x7BC110, (void *)a_743720, "079 sub_7BC110" },
		{ 0x7BC5C0, (void *)a_743C20, "079 sub_7BC5C0" },
		{ 0x7BC5F0, (void *)a_73A6D0, "079 nullsub_1384" },
		{ 0x7BC600, (void *)a_73A6D0, "079 nullsub_1385" },
		{ 0x7BCAE0, (void *)a_73A6D0, "079 nullsub_1386" },
		{ 0x7BCB30, (void *)a_73A6D0, "079 nullsub_1387" },
		{ 0x7BCB40, (void *)a_73A380, "079 sub_7BCB40" },
		{ 0x7BCD30, (void *)a_73A6D0, "079 nullsub_1388" },
		{ 0x7BCD40, (void *)a_73A6D0, "079 nullsub_1389" },
		{ 0x7BCD50, (void *)a_73A380, "079 sub_7BCD50" },
		{ 0x7BCDB0, (void *)a_7335D0, "079 sub_7BCDB0" },
		{ 0x7BCE30, (void *)a_73A6D0, "079 nullsub_1390" },
		{ 0x7BCE40, (void *)a_73A380, "079 sub_7BCE40" },
		{ 0x7BCF00, (void *)a_73A6D0, "079 nullsub_1391" },
		{ 0x7BDAD0, (void *)a_73A380, "079 sub_7BDAD0" },
		{ 0x7BDBD0, (void *)a_73A6D0, "079 nullsub_1392" },
		{ 0x7BDDF0, (void *)a_733950, "079 sub_7BDDF0" },
		{ 0x7BE0B0, (void *)a_73A6D0, "079 nullsub_1393" },
		{ 0x7BE100, (void *)a_7474B0, "079 MAG_079_sub_7BE100" },
		{ 0x7BE120, (void *)a_7474D0, "079 MAG_079_sub_7BE120" },
		{ 0x7BE140, (void *)a_73A6D0, "079 nullsub_1394" },
		{ 0x7BE150, (void *)a_747500, "079 MAG_079_sub_7BE150" },
		{ 0x7BE190, (void *)a_73A0D0, "079 MAG_079_sub_7BE190" },
		{ 0x7BE240, (void *)a_747560, "079 MAG_079_sub_7BE240" },
		{ 0x7BE270, (void *)a_747590, "079 MAG_079_sub_7BE270" },
		{ 0x7BE280, (void *)a_7475A0, "079 MAG_079_sub_7BE280" },
		{ 0x7BE2A0, (void *)a_73A6D0, "079 nullsub_1376" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (this file)
	static const ModPort PORTS[] = {
		{ 0x7B4D80, (void *)ud_7B4D80, "079 ud_7B4D80" },
		{ 0x7B4F40, (void *)ud_7B4F40, "079 ud_7B4F40" },
		{ 0x7B4F80, (void *)ud_7B4F80, "079 ud_7B4F80" },
		{ 0x7B5070, (void *)ud_7B5070, "079 ud_7B5070" },
		{ 0x7B50C0, (void *)ud_7B50C0, "079 ud_7B50C0" },
		{ 0x7B5520, (void *)ud_7B5520, "079 ud_7B5520" },
		{ 0x7B55B0, (void *)ud_7B55B0, "079 ud_7B55B0" },
		{ 0x7B5650, (void *)ud_7B5650, "079 ud_7B5650" },
		{ 0x7B56A0, (void *)ud_7B56A0, "079 ud_7B56A0" },
		{ 0x7B5710, (void *)ud_7B5710, "079 ud_7B5710" },
		{ 0x7B5750, (void *)ud_7B5750, "079 ud_7B5750" },
		{ 0x7B57F0, (void *)ud_7B57F0, "079 ud_7B57F0" },
		{ 0x7B5880, (void *)ud_7B5880, "079 ud_7B5880" },
		{ 0x7B58E0, (void *)ud_7B58E0, "079 ud_7B58E0" },
		{ 0x7B5900, (void *)ud_7B5900, "079 ud_7B5900" },
		{ 0x7B5930, (void *)ud_7B5930, "079 ud_7B5930" },
		{ 0x7B59B0, (void *)ud_7B59B0, "079 ud_7B59B0" },
		{ 0x7B5B30, (void *)ud_7B5B30, "079 ud_7B5B30" },
		{ 0x7B5BE0, (void *)ud_7B5BE0, "079 ud_7B5BE0" },
		{ 0x7B5D80, (void *)ud_7B5D80, "079 ud_7B5D80" },
		{ 0x7B6020, (void *)ud_7B6020, "079 ud_7B6020" },
		{ 0x7B6150, (void *)ud_7B6150, "079 ud_7B6150" },
		{ 0x7B6370, (void *)ud_7B6370, "079 ud_7B6370" },
		{ 0x7B6600, (void *)ud_7B6600, "079 ud_7B6600" },
		{ 0x7B6A80, (void *)ud_7B6A80, "079 ud_7B6A80" },
		{ 0x7B7530, (void *)ud_7B7530, "079 ud_7B7530" },
		{ 0x7B7A30, (void *)ud_7B7A30, "079 ud_7B7A30" },
		{ 0x7B8080, (void *)ud_7B8080, "079 ud_7B8080" },
		{ 0x7B8110, (void *)ud_7B8110, "079 ud_7B8110" },
		{ 0x7B8170, (void *)ud_7B8170, "079 ud_7B8170" },
		{ 0x7B8220, (void *)ud_7B8220, "079 ud_7B8220" },
		{ 0x7B8290, (void *)ud_7B8290, "079 ud_7B8290" },
		{ 0x7B8320, (void *)ud_7B8320, "079 ud_7B8320" },
		{ 0x7B8500, (void *)ud_7B8500, "079 ud_7B8500" },
		{ 0x7B8780, (void *)ud_7B8780, "079 ud_7B8780" },
		{ 0x7B8E30, (void *)ud_7B8E30, "079 ud_7B8E30" },
		{ 0x7B8EC0, (void *)ud_7B8EC0, "079 ud_7B8EC0" },
		{ 0x7B8F20, (void *)ud_7B8F20, "079 ud_7B8F20" },
		{ 0x7B9080, (void *)ud_7B9080, "079 ud_7B9080" },
		{ 0x7B9140, (void *)ud_7B9140, "079 ud_7B9140" },
		{ 0x7B91C0, (void *)ud_7B91C0, "079 ud_7B91C0" },
		{ 0x7B9200, (void *)ud_7B9200, "079 ud_7B9200" },
		{ 0x7BA230, (void *)ud_7BA230, "079 ud_7BA230" },
		{ 0x7BA340, (void *)ud_7BA340, "079 ud_7BA340" },
		{ 0x7BA3C0, (void *)ud_7BA3C0, "079 ud_7BA3C0" },
		{ 0x7BA3F0, (void *)ud_7BA3F0, "079 ud_7BA3F0" },
		{ 0x7BA490, (void *)ud_7BA490, "079 ud_7BA490" },
		{ 0x7BA510, (void *)ud_7BA510, "079 ud_7BA510" },
		{ 0x7BA590, (void *)ud_7BA590, "079 ud_7BA590" },
		{ 0x7BAD70, (void *)ud_7BAD70, "079 ud_7BAD70" },
		{ 0x7BAF90, (void *)ud_7BAF90, "079 ud_7BAF90" },
		{ 0x7BB680, (void *)ud_7BB680, "079 ud_7BB680" },
		{ 0x7BB970, (void *)ud_7BB970, "079 ud_7BB970" },
		{ 0x7BBAD0, (void *)ud_7BBAD0, "079 ud_7BBAD0" },
		{ 0x7BBB60, (void *)ud_7BBB60, "079 ud_7BBB60" },
		{ 0x7BBBF0, (void *)ud_7BBBF0, "079 ud_7BBBF0" },
		{ 0x7BBC80, (void *)ud_7BBC80, "079 ud_7BBC80" },
		{ 0x7BBCA0, (void *)ud_7BBCA0, "079 ud_7BBCA0" },
		{ 0x7BBCD0, (void *)ud_7BBCD0, "079 ud_7BBCD0" },
		{ 0x7BBDF0, (void *)ud_7BBDF0, "079 ud_7BBDF0" },
		{ 0x7BBEB0, (void *)ud_7BBEB0, "079 ud_7BBEB0" },
		{ 0x7BC580, (void *)ud_7BC580, "079 ud_7BC580" },
		{ 0x7BC5A0, (void *)ud_7BC5A0, "079 ud_7BC5A0" },
		{ 0x7BC610, (void *)ud_7BC610, "079 ud_7BC610" },
		{ 0x7BC670, (void *)ud_7BC670, "079 ud_7BC670" },
		{ 0x7BC6B0, (void *)ud_7BC6B0, "079 ud_7BC6B0" },
		{ 0x7BC720, (void *)ud_7BC720, "079 ud_7BC720" },
		{ 0x7BC840, (void *)ud_7BC840, "079 ud_7BC840" },
		{ 0x7BC950, (void *)ud_7BC950, "079 ud_7BC950" },
		{ 0x7BCAA0, (void *)ud_7BCAA0, "079 ud_7BCAA0" },
		{ 0x7BCAC0, (void *)ud_7BCAC0, "079 ud_7BCAC0" },
		{ 0x7BCAF0, (void *)ud_7BCAF0, "079 ud_7BCAF0" },
		{ 0x7BCBA0, (void *)ud_7BCBA0, "079 ud_7BCBA0" },
		{ 0x7BCBC0, (void *)ud_7BCBC0, "079 ud_7BCBC0" },
		{ 0x7BCBF0, (void *)ud_7BCBF0, "079 ud_7BCBF0" },
		{ 0x7BCC80, (void *)ud_7BCC80, "079 ud_7BCC80" },
		{ 0x7BCCF0, (void *)ud_7BCCF0, "079 ud_7BCCF0" },
		{ 0x7BCD10, (void *)ud_7BCD10, "079 ud_7BCD10" },
		{ 0x7BCDF0, (void *)ud_7BCDF0, "079 ud_7BCDF0" },
		{ 0x7BCEA0, (void *)ud_7BCEA0, "079 ud_7BCEA0" },
		{ 0x7BCEC0, (void *)ud_7BCEC0, "079 ud_7BCEC0" },
		{ 0x7BCF10, (void *)ud_7BCF10, "079 ud_7BCF10" },
		{ 0x7BD020, (void *)ud_7BD020, "079 ud_7BD020" },
		{ 0x7BD110, (void *)ud_7BD110, "079 ud_7BD110" },
		{ 0x7BD800, (void *)ud_7BD800, "079 ud_7BD800" },
		{ 0x7BD890, (void *)ud_7BD890, "079 ud_7BD890" },
		{ 0x7BD900, (void *)ud_7BD900, "079 ud_7BD900" },
		{ 0x7BD9B0, (void *)ud_7BD9B0, "079 ud_7BD9B0" },
		{ 0x7BDA60, (void *)ud_7BDA60, "079 ud_7BDA60" },
		{ 0x7BDB30, (void *)ud_7BDB30, "079 ud_7BDB30" },
		{ 0x7BDB90, (void *)ud_7BDB90, "079 ud_7BDB90" },
		{ 0x7BDBE0, (void *)ud_7BDBE0, "079 ud_7BDBE0" },
		{ 0x7BDCB0, (void *)ud_7BDCB0, "079 ud_7BDCB0" },
		{ 0x7BDD00, (void *)ud_7BDD00, "079 ud_7BDD00" },
		{ 0x7BDD40, (void *)ud_7BDD40, "079 ud_7BDD40" },
		{ 0x7BDD70, (void *)ud_7BDD70, "079 ud_7BDD70" },
		{ 0x7BDEF0, (void *)ud_7BDEF0, "079 ud_7BDEF0" },
		{ 0x7BDF90, (void *)ud_7BDF90, "079 ud_7BDF90" },
		{ 0x7BE050, (void *)ud_7BE050, "079 ud_7BE050" },
		{ 0x7BE0C0, (void *)ud_7BE0C0, "079 ud_7BE0C0" },
		{ 0x7BE1A0, (void *)ud_7BE1A0, "079 ud_7BE1A0" },
		{ 0x7BE1E0, (void *)ud_7BE1E0, "079 ud_7BE1E0" },
		{ 0x7BE1F0, (void *)ud_7BE1F0, "079 ud_7BE1F0" },
		{ 0x7BE220, (void *)ud_7BE220, "079 ud_7BE220" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag079_ultimecia_final_death()
	{
		act::register_module(79);
		for (const act::udeath::ModPort *p = act::udeath::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(79, p->addr, p->port, p->name);
		for (const act::udeath::ModPort *p = act::udeath::PORTS; p->addr; p++)
			act::register_module_port(79, p->addr, p->port, p->name);
		// 30 fps layer: see mag079_ultimecia_final_death_held.inc
		FX_HELD(register_mag079_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag079_ultimecia_final_death_held.inc"
#endif
