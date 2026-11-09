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

// Effect 237: Explosion (enemy attacks 128 / 129 / 130 of kernel.bin, Death Claw c0m035), cinematic-engine module MAG_237 0xA29C00..0xA34210,
// script data magic/mag236_b.00/.01 (loaded by MAG_237_UNKNOWN_FL 0xA29C00: IO_GetFile_MAGIC of the names
// 0x186654C / 0x1866558; setup MAG_237_UNKNOWN 0xA29C30 = effect table index 236).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x1865E88; its descriptor block 0x1865B68 sits 0x320 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcX\tools\cmpall.py / best.py / callees.py):
//   - SetupSummon 0xA29C90, BindContext 0xA29CE0, the section pointers 0xA29D80, InitSequenceState
//     0xA33820, SequenceTick 0xA2A2A0 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xA341F0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (as BGH251F2's
//     turret explosion 244, plus the generic prims 7 / 11), except
//       VM 0x006 LoadBattleFile 0xA2F950: same code, own load callback 0xA2FA80 (c.load_cb),
//       bone 4 0xA2A950 OrbitBetweenBones = Disease Breath 231's 0xA671C0,
//       bone 8 0xA2D400 VertexAtHeight, prim 48 0xA2C9D0 TintedFT3Abr, draw 31 0xA2D000 MeshHeightCut
//         (+ its block renderer 0xA2D220): Death Claw's own handlers, the same code in Gas 236 and
//         Explosion 237: gfc_deathclaw.inc.
//       VM 0x042 0xA33670 = Bahamut 0xB24BB0 EntityChainAnim (apply_shared_misc: only that slot taken),
//       VM 0x112 0xA32E10 SetSpeedFromDistance, VM 0x113 0xA32F50 SetAngleTowardBone, prim 57 0xA2CC50
//         TintedG4Abr: Explosion's own handlers, ported here.
// The effect's queue (0x2796940) holds one task, the engine tick 0xA2A2A0.

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace explosion237
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag237_explosion_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace explosion237
{
	static void describe(Clone &c)
	{
		c.name = "237 Explosion";
		c.effect_id = 237;
		c.vm_table = 0x186602C;      // MAG_237 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1865C9C;    // MAG_237 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1865CE4;    // bone_table + 0x48
		c.prim_size = 0x1865DD4;     // bone_table + 0x138
		c.spr_table = 0x1865DE8;     // bone_table + 0x14C
		c.ptype_table = 0x1865E14;   // bone_table + 0x178
		c.pop_table = 0x1865E28;     // bone_table + 0x18C
		c.draw_table = 0x1865E88;    // MAG_237 DrawHandlerTable
		c.attr_16A = 0x1865FF2;
		c.attr_184 = 0x186600C;
		c.attr_186 = 0x186600E;
		c.attr_194 = 0x186601C;
		c.attr_19C = 0x1866024;
		c.desc = 0x1865B68;          // draw_table - 0x320 (read by 0xA29D80)
		c.ptr_block = 0x1865C74;     // draw_table - 0x214 (read by 0xA29CE0)
		c.queue = 0x2796940;
		c.file00 = 0x2796954;
		c.file01 = 0x2796950;
		c.tick = 0xA2A2A0;
		c.load_cb = 0xA2FA80;        // passed by VM 0x006 0xA2F950
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}


	// prim 57: the never-written local [ebp-0x1c] vanilla stores into ws+0xF0 (Explosion only)
	static const uint32_t PRIM57_F0 = 0;
#include "gfc_deathclaw.inc"

	// ------------------------------------------------------------------------------------
	// Explosion 237's own handlers
	// ------------------------------------------------------------------------------------
	// 0x56BEC0 Math_SqrtInt
	static inline int32_t xl_ISqrt(int32_t v) { return x::f<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }

	// 0xA32E10 (VM 0x112 SetSpeedFromDistance): operands {s16 w, s16 bone A, s16 bone B, s16 divisor}:
	// d = outPos(B) - outPos(A). Without opcode bit 0x8000 (RT+0x4B & 0x80): S32 bone+0x50 + 4 w =
	// ((|d| << 16) / divisor) << 8 (|d| = sqrt(sqrt(dx^2 + dy^2)^2 + dz^2), 0 when |d| is 0; w taken
	// whole). Always: S32 bone+0x50 + 4 (w & 7) = ((sqrt(the squares of the axes w bits 8 / 9 / 10 select)
	// << 16) / divisor) << 8; then outAngle = HIWORD(accumRot) (0xB2C440); cursor += 10.
	static void __cdecl op_112_SetSpeedFromDistance()
	{
		uint8_t *a = blob::GetBone(S16(STREAM(), 4));                       // ebx / [ebp-4]
		uint8_t *b = blob::GetBone(S16(STREAM(), 6));
		const int32_t dx = S16(b, 0x94) - S16(a, 0x94);                     // esi
		const int32_t dy = S16(b, 0x96) - S16(a, 0x96);                     // edi
		const int32_t dz = S16(b, 0x98) - S16(a, 0x98);                     // ebx
		if (!(U8(RT(), 0x4B) & 0x80))
		{
			const int32_t h = xl_ISqrt(add32(mul32(dy, dy), mul32(dx, dx)));
			int32_t r = xl_ISqrt(add32(mul32(h, h), mul32(dz, dz)));
			if (r != 0) r = shl32(r, 16) / S16(STREAM(), 8);
			const int32_t w = S16(STREAM(), 2);
			FX_HELD(guard_bone(CUR(), 0x50 + 4 * w, 4);)
			S32(CUR(), 0x50 + 4 * w) = shl32(r, 8);
		}
		const int32_t w = S16(STREAM(), 2);                                 // [ebp-4]
		int32_t sq = 0;
		if (w & 0x100) sq = mul32(dx, dx);
		if (w & 0x200) sq = add32(sq, mul32(dy, dy));
		if (w & 0x400) sq = add32(sq, mul32(dz, dz));
		const int32_t v = shl32(xl_ISqrt(sq), 16) / S16(STREAM(), 8);
		S32(CUR(), 0x50 + 4 * (w & 7)) = shl32(v, 8);
		h_B2C440();
		STREAM() = STREAM() + 10;
	}

	// 0xA32F50 (VM 0x113 SetAngleTowardBone): operands {s16 w, s16 bone A, s16 bone B}: S32 bone+0x50 +
	// 4 w = 0xB66B30(A.x, A.z, B.x, B.z) << 16 (direction A -> B in the XZ plane); 4 w >= 0xC (an accumPos
	// word or beyond): the bone's own handler (bone table[bone+0x18]) runs, else outAngle =
	// HIWORD(accumRot) (0xB2C440); cursor += 8.
	static void __cdecl op_113_SetAngleTowardBone()
	{
		uint8_t *a = blob::GetBone(S16(STREAM(), 4));                       // esi
		uint8_t *b = blob::GetBone(S16(STREAM(), 6));
		const int32_t ang = blob::Blob_B66B30(S16(a, 0x94), S16(a, 0x98), S16(b, 0x94), S16(b, 0x98));
		const int32_t off = shl32(S16(STREAM(), 2), 2);
		FX_HELD(guard_bone(CUR(), 0x50 + off, 4);)
		S32(CUR(), 0x50 + off) = shl32(ang, 16);
		if (off >= 0xC) C().bone[U8(CUR(), 0x18)]();
		else h_B2C440();
		STREAM() = STREAM() + 8;
	}

	// 0xA2CC50 (prim 57 TintedG4Abr, type 17 with drawFlags bit 2). Record 0x18 (+0/+4/+8/+0xC u32
	// colour 0..3 as the generic G4, +0x10..+0x16 u16 vertex offsets). Packet 0x2C (len 0xA): +4 the
	// draw-mode word ws+0x50, +8/+0x10/+0x18/+0x20 colours, +0xC/+0x14/+0x1C/+0x24 SXY, +0x28 draw mode
	// 0xE1000220. Vanilla as compiled: colour 0 = NCCS of (ws+0x8C | colour 0 | 0x38000000), colours 1
	// and 2 = NCCS of the ADDRESSES rec + 4 / rec + 8, colour 3 = NCCS of 0xC, and the OT slot =
	// ((sum of the 4 depth words) >> 2 & 0x3FFC) + 0xC with no OT base (SSIGPU_InsertPrimDepthKeys then
	// writes near address 0: the renderer faults if it ever draws; Explosion's script never selects
	// it). ws+0xF0 receives the never-written local [ebp-0x1c] (PRIM57_F0).
	static void __cdecl pr_57_TintedG4Abr()
	{
		h_B29450((int32_t)U32(WS(), 0x84));
		uint8_t *ws = WS();                                                  // [ebp-0x18]
		uint8_t *pkt = PTR(ws, 0x60);                                        // esi
		uint8_t *rec = PTR(ws, 0x6C);                                        // ebx
		int32_t count = S32(ws, 0x70);                                       // [ebp-0x20]
		U32(ws, 0xF0) = PRIM57_F0;
		const uint32_t nocull = U32(ws, 0x90) & 0x10;                        // [ebp-0x1c]
		do
		{
			const uint32_t o0 = U16(rec, 0x10), o1 = U16(rec, 0x12), o2 = U16(rec, 0x14), o3 = U16(rec, 0x16);
			const int32_t z0 = DcDepthKey(o0);
			const int32_t z1 = DcDepthKey(o1);
			const int32_t z2 = DcDepthKey(o2);
			const int32_t z3 = DcDepthKey(o3);
			uint8_t *vb = PTR(ws, 0x7C);
			uint8_t *p0 = vb + o0, *p1 = vb + o1, *p2 = vb + o2, *p3 = vb + o3;
			const uint32_t s0 = U32(p0, 0), s1 = U32(p1, 0), s2 = U32(p2, 0), s3 = U32(p3, 0);
			U32(pkt, 0xC) = s0;
			U32(pkt, 0x1C) = s2;
			U32(pkt, 0x24) = s3;
			U32(pkt, 0x14) = s1;
			if (nocull == 0)
			{
				DcGteData(12) = s0;
				DcGteData(13) = s1;
				DcGteData(14) = s2;
				x::GteNCLIP();
				if ((int32_t)DcGteData(24) < 0) goto next;
			}
			{
				const uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4), a3 = U32(p3, 4);
				if ((a3 | a2 | a1 | a0) & 0x460000) goto next;
				U8(pkt, 3) = 0xA;
				U32(pkt, 0x28) = 0xE1000220;
				const uint32_t slot = (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC) + 0xC;
				DcGteData(6) = U32(ws, 0x8C) | U32(rec, 0) | 0x38000000;
				const uint32_t dm = U32(ws, 0x50);
				x::Gte_4601B0();
				U32(pkt, 4) = dm;
				U32(pkt, 8) = DcGteData(22);
				DcGteData(6) = (uint32_t)(uintptr_t)(rec + 4);
				x::Gte_4601B0();
				U32(pkt, 0x10) = DcGteData(22);
				DcGteData(6) = (uint32_t)(uintptr_t)(rec + 8);
				x::Gte_4601B0();
				U32(pkt, 0x18) = DcGteData(22);
				DcGteData(6) = 0xC;
				x::Gte_4601B0();
				U32(pkt, 0x20) = DcGteData(22);
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				pkt += 0x2C;
			}
		next:
			rec += 0x18;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// VM 0x042 is the only shared handler the module carries: take that one slot from
	// apply_shared_misc and keep every other slot as init_clone built it (as effect 248)
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn v042 = c.vm[0x042];    // 0xA33670 = Bahamut 0xB24BB0 EntityChainAnim
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.vm[0x042] = v042;
		c.vm[0x112] = op_112_SetSpeedFromDistance;      // 0xA32E10
		c.vm[0x113] = op_113_SetAngleTowardBone;        // 0xA32F50
		c.prim[57] = pr_57_TintedG4Abr;                 // 0xA2CC50
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag237_explosion()
	{
		using namespace gfc;
		explosion237::describe(explosion237::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<237>::Register("Explosion (Death Claw)", explosion237::g_c, 0xA29C00, 0x186654C, 0x1866558,
		                           (void *)explosion237::SequenceTask);
		init_clone(explosion237::g_c, nullptr, 0);
		explosion237::g_c.bone[4] = explosion237::bh_04_OrbitBetweenBones;     // 0xA2A950
		explosion237::g_c.bone[8] = explosion237::bh_08_VertexAtHeight;        // 0xA2D400
		explosion237::g_c.prim[48] = explosion237::pr_48_TintedFT3Abr;         // 0xA2C9D0
		explosion237::g_c.draw[31] = explosion237::dh_31_MeshHeightCut;        // 0xA2D000
		explosion237::apply_slots(explosion237::g_c);
		register_port(0xA2A2A0, (void *)explosion237::SequenceTask, "MAG237 Explosion SequenceTick", 237);
		// 30 fps layer: see mag237_explosion_held.inc
		FX_HELD(register_mag237_explosion_held();)
	}
	FX_LAB_MODULE(register_mag237_explosion);
}

#ifdef FF8_FX_HELD
#include "mag237_explosion_held.inc"
#endif
