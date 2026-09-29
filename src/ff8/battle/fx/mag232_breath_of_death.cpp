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

// Effect 232: Breath of Death (enemy attack 64 of kernel.bin, used by Blood Soul c0m025;
// cinematic-engine attack, module MAG_232 0xA5C2B0..0xA66470, script data magic/mag231_b.00/.01,
// loaded by MAG_232_UNKNOWN_FL 0xA5C2B0).
//
// One more compile of the GF cinematic engine (gfc_engine.cpp), a sibling of Disease Breath
// (mag231_disease_breath.cpp) and Breath (mag235_breath.cpp). Instruction-level comparison
// against the GF clones, Water, Meteor and Breath (module-relative addresses normalised, callee
// trees hashed): BindContext 0xA5C390, ReleaseContext 0xA5C420, SequenceTick 0xA5C950 (the unlit
// variant = Brothers 0xAF4B90), AnimIntegrator 0xA5CC70 and every table handler are the generic
// (Ifrit) ones - VM 0x006 LoadBattleFile 0xA61D70 has its own load callback 0xA61EA0 (c.load_cb),
// VM 0x042 0xA658D0 is EntityChainAnim (= Bahamut 0xB24BB0 / Brothers 0xAFEF60, apply_shared_misc)
// - except the three handlers only this module (and partly Disease Breath) has, ported here:
//   bone 10 0xA5D100 ArcBetweenBones (compiled out in every other clone): two reference bones A / B
//     and a radius (*(s16 *)bone+0xA0 [0] / [1] / [2]); angle = 0xB66B30(B.x, B.z, A.x, A.z)
//     (direction B -> A in the XZ plane); P = B + radius * (sin, cos)(angle) in XZ (4.12, sar 12);
//     outPos X / Z = A + (P - A) * HIWORD(accumRot X / Z) / 256 + HIWORD(accumPos X / Z),
//     outPos Y = A.y + (B.y - A.y) * HIWORD(accumRot Y) / 256 + HIWORD(accumPos Y);
//   prim 58 0xA5F610 (type 18 with drawFlags bit 2: FT4 semi-transparent, a byte clone of Disease
//     Breath 0xA69D60): the generic tinted FT4 renderer (0xB28DD0) with a 0x2C-byte packet (len 10)
//     whose last word is a draw-mode command 0xE1000220 (semi-transparency mode 1), the lit colour
//     read from the GTE (RGB2) after the OT insertion; its colour setup 0xA5F8F0 = the generic
//     0xB29450;
//   draw 44 0xA5D730 AnimatedEntityCopy: a battle entity (the caster: SceneHeader+0x60 +
//     4 * bone[0x1B]) redrawn as an embedded battle model with its own animation. First call
//     (bone+0xBC == 0): the entity model copy block (0xA5D8F0 = Brothers 0xAF62B0, sync 0xA5D950
//     = Brothers 0xAF6310) at bone+0xBC, a private copy of the entity skeleton in the arena
//     (0x10 + 0x30 * bone count bytes, block+0x30 -> it, header +4 -> block+0x30) and the
//     animation *(s16 *)bone+0xB8 read into the block's command (0x509440). Every call: as the
//     generic draw 3 (parent xform scaled 0xB269E0, model step / pose 0xB26AD0, RenderGeometry
//     mode 4) with the render colour block+0x5C = per channel clamp(entity tint (block+0x78) +
//     bone+0xCC - 0x80, 0..255) | (bone+0xCC & 0x2000000).
// Draw 2 0xA5DE00 (MeshScaled) and its scaled parent transform 0xA5D580 are byte clones of the
// generic ones (0xB27050 / 0xB269E0); this file carries its own copies (used by draws 2 and 44).
// The effect's queue (0x2796A58) holds one task, the engine tick 0xA5C950. The effect draws the
// caster's model with an animation of that model: the original faults when the caster's model
// lacks it (the harness runs it with Blood Soul's model c0m025).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace death232
{
	static Clone g_c = {};

	static void describe(Clone &c)
	{
		c.name = "232 Breath of Death";
		c.effect_id = 232;
		c.vm_table = 0x1869118;     // VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1868D88;   // BoneHandlerTable
		c.prim_table = 0x1868DD0;   // bone_table + 0x48
		c.prim_size = 0x1868EC0;    // bone_table + 0x138
		c.spr_table = 0x1868ED4;    // bone_table + 0x14C
		c.ptype_table = 0x1868F00;  // bone_table + 0x178
		c.pop_table = 0x1868F14;    // bone_table + 0x18C
		c.draw_table = 0x1868F74;   // DrawHandlerTable
		c.attr_16A = 0x18690DE;
		c.attr_184 = 0x18690F8;
		c.attr_186 = 0x18690FA;
		c.attr_194 = 0x1869108;
		c.attr_19C = 0x1869110;
		c.desc = 0x1868C88;         // draw_table - 0x2EC
		c.ptr_block = 0x1868D60;    // draw_table - 0x214
		c.queue = 0x2796A58;
		c.file00 = 0x2796A6C;
		c.file01 = 0x2796A68;
		c.tick = 0xA5C950;
		c.load_cb = 0xA61EA0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}
}
}
}

#ifdef FF8_FX_HELD
#include "mag232_breath_of_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace death232
{
	// GTE data registers (0x1CA8A10 + 4 * reg)
	inline uint32_t &GteData(int reg) { return MEM<uint32_t>(0x1CA8A10 + 4 * reg); }

	// ------------------------------------------------------------------------------------
	// 0xA5D100 (bone 10 ArcBetweenBones): see the module comment
	// ------------------------------------------------------------------------------------
	static void __cdecl bh_10_ArcBetweenBones()
	{
		uint8_t *refs = PTR(CUR(), 0xA0);                                   // esi
		uint8_t *a = blob::GetBone(S16(refs, 0));                           // edi
		uint8_t *b = blob::GetBone(S16(refs, 2));                           // ebx
		int32_t rad = S16(refs, 4);                                         // [ebp-4]
		int32_t ang = blob::Blob_B66B30(S16(b, 0x94), S16(b, 0x98), S16(a, 0x94), S16(a, 0x98)); // [ebp-8]
		int32_t pz = add32(mul32(x::Cos(ang), rad) >> 12, S16(b, 0x98));   // esi
		int32_t px = add32(mul32(x::Sin(ang), rad) >> 12, S16(b, 0x94));
		int32_t ax = S16(a, 0x94);                                          // cx
		uint8_t *bone = CUR();
		int32_t d = mul32(sub32(px, ax), S32(bone, 0x50) >> 16) / 256;
		S16(bone, 0x94) = (int16_t)add32(S32(bone, 0x5C) >> 16, add32(ax, d));
		int32_t ay = S16(a, 0x96);                                          // cx
		int32_t by = S16(b, 0x96);
		bone = CUR();                                                       // ebx
		d = mul32(sub32(by, ay), S32(bone, 0x54) >> 16) / 256;
		S16(bone, 0x96) = (int16_t)add32(S32(bone, 0x60) >> 16, add32(ay, d));
		int32_t az = S16(a, 0x98);                                          // di
		bone = CUR();                                                       // ecx
		d = mul32(sub32(pz, az), S32(bone, 0x58) >> 16) / 256;
		S16(bone, 0x98) = (int16_t)add32(S32(bone, 0x64) >> 16, add32(az, d));
	}

	// fild qword [u16 DEPTHARR[off >> 3]]; fmul dword [0x1877DA8]; call __ftol (low dword kept;
	// u16 x float = at most 40 significant bits, exact in a double)
	static inline int32_t DepthKey(uint32_t off)
	{
		uint32_t v = DEPTHARR()[(int32_t)off / 8] & 0xFFFF;
		return (int32_t)(int64_t)((double)v * (double)FLT_1877DA8());
	}

	// ------------------------------------------------------------------------------------
	// 0xA5F610 (prim 58): semi-transparent tinted FT4. Record 0x18 as the generic FT4 (+0 u32
	// colour, +4..+0xA u16 UV 0..3, +0xC..+0x12 u16 vertex offsets, +0x14 CLUT, +0x16 tpage).
	// Packet 0x2C (len 0xA): +4 RGB (lit, code 0x2C | ws+0x8C), +8/+0x10/+0x18/+0x20 SXY,
	// +0xC/+0x14/+0x1C/+0x24 UV + ws+0x98, +0xE CLUT + ws+0x54, +0x16 tpage | ws+0x50,
	// +0x28 draw mode 0xE1000220.
	// ------------------------------------------------------------------------------------
	static void __cdecl pr_58_TintedFT4Abr()
	{
		h_B29450((int32_t)U32(WS(), 0x84));                                 // 0xA5F8F0
		uint8_t *ws = WS();                                                 // [ebp-0x10]
		uint8_t *pkt = PTR(ws, 0x60);                                       // esi
		uint8_t *rec = PTR(ws, 0x6C);                                       // edi
		int32_t count = S32(ws, 0x70);                                      // [ebp-0x1c]
		uint32_t ot = U32(ws, 0x5C);                                        // [ebp-0x24]
		U32(ws, 0xF0) = 0;
		uint32_t nocull = U32(ws, 0x90) & 0x10;                             // [ebp-0x20]
		do
		{
			uint32_t o0 = U16(rec, 0xC);                                    // ebx
			uint32_t o1 = U16(rec, 0xE);                                    // [ebp-0x14]
			uint32_t o2 = U16(rec, 0x10);                                   // [ebp-4]
			uint32_t o3 = U16(rec, 0x12);                                   // [ebp-8]
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			int32_t z3 = DepthKey(o3);
			uint8_t *vb = PTR(ws, 0x7C);
			uint8_t *p0 = vb + o0;
			uint8_t *p1 = vb + o1;
			uint8_t *p2 = vb + o2;
			uint8_t *p3 = vb + o3;
			uint32_t s0 = U32(p0, 0);
			uint32_t s1 = U32(p1, 0);
			uint32_t s2 = U32(p2, 0);
			uint32_t s3 = U32(p3, 0);
			U32(pkt, 8) = s0;
			U32(pkt, 0x18) = s2;
			U32(pkt, 0x20) = s3;
			U32(pkt, 0x10) = s1;
			if (nocull == 0)
			{
				GteData(12) = s0;
				GteData(13) = s1;
				GteData(14) = s2;
				x::GteNCLIP();
				if ((int32_t)GteData(24) < 0) goto next;
			}
			{
				uint32_t a0 = U32(p0, 4), a2 = U32(p2, 4), a1 = U32(p1, 4), a3 = U32(p3, 4);
				if ((a3 | a2 | a1 | a0) & 0x460000) goto next;
				U8(pkt, 3) = 0xA;
				U32(pkt, 0x28) = 0xE1000220;
				uint32_t slot = (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC) + ot;
				U16(pkt, 0x16) = (uint16_t)(U32(ws, 0x50) | U16(rec, 0x16));
				U16(pkt, 0xE) = (uint16_t)(U16(rec, 0x14) + (uint16_t)U32(ws, 0x54));
				uint32_t uvofs = U16(ws, 0x98);
				uint32_t uv0 = U16(rec, 4) + uvofs;
				uint32_t uv1 = U16(rec, 6) + uvofs;
				uint32_t uv2 = U16(rec, 8) + uvofs;
				uint32_t uv3 = U16(rec, 0xA) + uvofs;
				U16(pkt, 0xC) = (uint16_t)uv0;
				U16(pkt, 0x24) = (uint16_t)uv3;
				U16(pkt, 0x14) = (uint16_t)uv1;
				U16(pkt, 0x1C) = (uint16_t)uv2;
				GteData(6) = U32(ws, 0x8C) | U32(rec, 0) | 0x2C000000;
				x::Gte_4601B0();
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				U32(pkt, 4) = GteData(22);
				pkt += 0x2C;
			}
		next:
			rec += 0x18;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// ------------------------------------------------------------------------------------
	// 0xA5D950 SyncEntityModelCopy(blk) (= Brothers 0xAF6310): blk+0x10..0x1B = entity+0x60..0x6B
	// (BattleAnimHeader: +0x14 = the entity's model pointer P), blk+0x30..0x3F = the 16 bytes at P
	// (entity = blk+0xC).
	// ------------------------------------------------------------------------------------
	static void SyncEntityModelCopy(uint8_t *blk)
	{
		uint8_t *src = PTR(blk, 0xC) + 0x60;
		uint8_t *d = blk + 0x10;
		uint8_t *d2 = blk + 0x30;
		U32(d, 0) = U32(src, 0);
		U32(d, 4) = U32(src, 4);
		U32(d, 8) = U32(src, 8);
		uint8_t *m = PTR(src, 4);
		U32(d2, 0) = U32(m, 0);
		U32(d2, 4) = U32(m, 4);
		U32(d2, 8) = U32(m, 8);
		U32(d2, 0xC) = U32(m, 0xC);
	}

	// ------------------------------------------------------------------------------------
	// 0xA5D8F0 CreateEntityModelCopy(ent) (= Brothers 0xAF62B0): 0x80-byte block from the arena:
	// +0xC = ent, sync (0xA5D950), render descriptor at +0x40: +0x50 = -1, +0x54 = 0,
	// +0x58 = 0xD00140, +0x5C = +0x78 = ent+0x28, +0x60 = ent+0x7C (object mask), +0x64 (u16) = 0,
	// +0x68 = 0; +0 / +2 (u16) = 0, +0x21 (u8) = 0. Returns the block.
	// ------------------------------------------------------------------------------------
	static uint8_t *CreateEntityModelCopy(uint8_t *ent)
	{
		uint8_t *blk = blob::ArenaAlloc(0x80);
		PTR(blk, 0xC) = ent;
		SyncEntityModelCopy(blk);
		U32(blk, 0x54) = 0;
		U32(blk, 0x58) = 0xD00140;
		uint32_t t = U32(ent, 0x28);
		U32(blk, 0x78) = t;
		U32(blk, 0x5C) = t;
		t = U32(ent, 0x7C);
		U16(blk, 0x64) = 0;
		U32(blk, 0x60) = t;
		U32(blk, 0x68) = 0;
		U32(blk, 0x50) = 0xFFFFFFFF;
		U16(blk, 0) = 0;
		U16(blk, 2) = 0;
		U8(blk, 0x21) = 0;
		return blk;
	}

	// ------------------------------------------------------------------------------------
	// 0xA5D580 SetupParentXformScaled (= generic 0xB269E0, gfc_engine.cpp h_B269E0): GTE R/TR =
	// parent matrix (bone+0x9C), TR = parent * bone position (bone+0x94); ws+0xD4.. = scale
	// bone+0x8C/8E/90 << 4; ws+0xE0 = parent scaled; GTE R = ws+0xE0. This module's copy, used by
	// its draw 2 and draw 44.
	// ------------------------------------------------------------------------------------
	static void SetupParentXformScaled()
	{
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));   // esi
		x::GteSetRotMatrix(m);
		x::GteSetTransVector(m);
		x::GteLoadV0(CUR() + 0x94);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
		S32(WS(), 0xD4) = shl32(S16(CUR(), 0x8C), 4);
		S32(WS(), 0xD8) = shl32(S16(CUR(), 0x8E), 4);
		S32(WS(), 0xDC) = shl32(S16(CUR(), 0x90), 4);
		// 30 fps layer: see mag232_breath_of_death_held.inc
		FX_HELD(held_scale_fraction(CUR(), WS() + 0xD4);)
		uint8_t *d = WS() + 0xE0;                                 // edi
		for (int i = 0; i < 8; i++) U32(d, i * 4) = U32(m, i * 4);
		x::ScaleMatrix(d, WS() + 0xD4);
		x::GteSetRotMatrix(d);
	}

	// ------------------------------------------------------------------------------------
	// 0xA5DE00 (draw 2 MeshScaled, = generic 0xB27050): mesh with the parent rotation scaled by
	// bone+0x8C.. (0xA5D580)
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_02_MeshScaled()
	{
		U32(WS(), 0x4C) = U32(CUR(), 0xBC);
		S32(WS(), 0x48) = 0;
		S32(WS(), 0x44) = 0;
		S32(WS(), 0x40) = 0;
		SetupParentXformScaled();
		h_B27440(nullptr);                                        // DrawMeshObject
	}

	// clamp(a + b - 0x80) to 0..0xFF (0xA5D827..0xA5D83E)
	static inline uint32_t AddTint(uint32_t a, uint32_t b)
	{
		int32_t v = (int32_t)(a + b) - 0x80;
		if (v >= 0x100) return 0xFF;
		if (v < 0) return 0;
		return (uint32_t)v;
	}

	// ------------------------------------------------------------------------------------
	// 0xA5D730 (draw 44 AnimatedEntityCopy): see the module comment
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_44_AnimatedEntityCopy()
	{
		if (U32(CUR(), 0xBC) == 0)
		{
			uint8_t *ent = PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B));
			PTR(CUR(), 0xBC) = CreateEntityModelCopy(ent);
			uint8_t *blk = PTR(CUR(), 0xBC);                                // edi
			uint8_t *old = PTR(blk, 0x30);                                  // [ebp-4]
			int32_t size = (int32_t)U8(old, 0) * 0x30 + 0x10;               // esi
			uint8_t *copy = blob::ArenaAlloc(size);
			PTR(blk, 0x30) = copy;
			if (size > 0)
			{
				uint32_t n = ((uint32_t)size + 3) >> 2;
				for (uint32_t k = 0; k < n; k++)
					U32(copy, 4 * k) = U32(old, 4 * k);
			}
			uint8_t *hdr = blk + 0x10;
			uint8_t *cmd = blk + 0x20;
			PTR(hdr, 4) = blk + 0x30;
			int32_t anim = S16(PTR(CUR(), 0xB8), 0);
			U8(blk, 9) = (uint8_t)anim;
			x::PreReadAnimation(hdr, cmd, anim);
		}
		uint8_t *blk = PTR(CUR(), 0xBC);                                    // ebx
		SetupParentXformScaled();                                           // 0xA5D580
		h_B26AD0(PTR(CUR(), 0xBC));                                         // 0xA5D670
		uint32_t tint = U32(blk, 0x78);                                     // [ebp-4]
		uint32_t col = U32(CUR(), 0xCC);                                    // esi, [ebp-8]
		uint32_t r = AddTint(tint & 0xFF, col & 0xFF);
		uint32_t g = AddTint((tint >> 8) & 0xFF, (col >> 8) & 0xFF);
		uint32_t b = AddTint((tint >> 16) & 0xFF, (col >> 16) & 0xFF);
		U32(blk, 0x5C) = (col & 0x2000000) | (b << 16) | r | (g << 8);
		uint8_t *m = PTR(CUR(), 0xBC);
		uint8_t *desc = m + 0x40;
		U32(desc, 4) = U32(CTX(), 0x74);
		uint32_t cursor = U32(CTX(), 0x7C);
		uint32_t ot = U32(RT(), 0x4C);
		// 30 fps layer: see mag232_breath_of_death_held.inc
		FX_HELD(held_render_begin();)
		U32(CTX(), 0x7C) = x::RenderGeometry(m + 0x30, desc, ot, 4, cursor);
		FX_HELD(held_render_end();)
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag232_breath_of_death()
	{
		using namespace gfc;
		death232::describe(death232::g_c);
		init_clone(death232::g_c, nullptr, 0);
		apply_shared_misc(death232::g_c);      // VM 0x042
		death232::g_c.draw[2] = death232::dh_02_MeshScaled;
		death232::g_c.bone[10] = death232::bh_10_ArcBetweenBones;
		death232::g_c.prim[58] = death232::pr_58_TintedFT4Abr;
		death232::g_c.draw[44] = death232::dh_44_AnimatedEntityCopy;
		register_port(0xA5C950, (void *)death232::SequenceTask, "MAG232 Breath of Death SequenceTick", 232);
		// 30 fps layer: see mag232_breath_of_death_held.inc
		FX_HELD(register_mag232_breath_of_death_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag232_breath_of_death_held.inc"
#endif
