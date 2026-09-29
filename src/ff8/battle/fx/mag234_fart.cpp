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

// Effect 234: Fart (enemy attack 110 of kernel.bin, used by Bite Bug c0m028; cinematic-engine
// attack, module MAG_234 0xA488C0..0xA51F10, script data magic/mag233_b.00/.01, loaded by
// MAG_234_UNKNOWN_FL 0xA488C0).
//
// Like Breath (mag235_breath.cpp) and Water (mag222_water.cpp), Fart is one more compile of the GF
// cinematic engine (gfc_engine.cpp): SetupSummon 0xA488F0, SequenceTick 0xA48F60 (the unlit
// variant = Brothers 0xAF4B90), BindContext 0xA489A0 are byte clones. Every table handler is the
// generic (Ifrit) one (VM 0x039 0xA4D5D0, 0x046 0xA4D110, 0x10C 0xA4DF80 and 0x13D 0xA4D450 only
// differ by the addresses in their switch tables; VM 0x006 LoadBattleFile 0xA4D890 has its own load
// callback 0xA4D9C0, c.load_cb) except
//   draw 30 0xA49C20 WobbleEntity, ported here: redraws a model with every vertex pushed sideways
//     by a sine wave of its height. First call: a 16-byte arena block at bone+0xC0 {+0 u16 0,
//     +2 u16 flags = param +0, +6 s16 reference node = param +4} (param block = bone+0xB8) and at
//     bone+0xBC either (flags bit 0) the embedded battle model of object param +2 (0xA498C0 =
//     Ifrit 0xB26860) or a model copy block of a live battle entity (the caster, SceneHeader +0x60
//     + 4 * bone[0x1B]; 0xA49E30 = Brothers 0xAF62B0, sync 0xA49E90 = Brothers 0xAF6310), the
//     entity's flag +0 |= 4. Every call: embedded model = parent xform + animation step (0xA49A40
//     / 0xA49B30 = Ifrit 0xB269E0 / 0xB26AD0); entity = its skeleton (and its weapon's, entity
//     +0x78) composed IN PLACE with camera node * entity matrix (0x5095B0), then the entity itself
//     (0x5088A0, mode 0x1F). Then a 256-entry table at the arena top: [i] = sin(phase + i * step) *
//     amplitude >> 12, [i + 0x200] = the cosine (reference node outPos: X = amplitude, Y = phase,
//     Z = step), and the model (and the weapon) drawn by 0xA49EE0: every visible object's vertex
//     groups through the bone matrices (0x56C850), x += table[(y >> 2) & 0xFF], z += the cosine
//     entry, projected with the identity billboard matrix 0x2797968 / zero vector 0x27971E4
//     (0x56C880), the object's primitives drawn by 0x5106E0 (mode 4, RT+0x4C).
// The effect's queue (0x27969E8) holds one task, the engine tick 0xA48F60.

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag234_fart_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace fart
{
	static Clone g_c = {};

	static void describe(Clone &c)
	{
		c.name = "234 Fart";
		c.effect_id = 234;
		c.vm_table = 0x1867DC0;     // VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1867A30;   // BoneHandlerTable
		c.prim_table = 0x1867A78;   // bone_table + 0x48
		c.prim_size = 0x1867B68;    // bone_table + 0x138
		c.spr_table = 0x1867B7C;    // bone_table + 0x14C
		c.ptype_table = 0x1867BA8;  // bone_table + 0x178
		c.pop_table = 0x1867BBC;    // bone_table + 0x18C
		c.draw_table = 0x1867C1C;   // DrawHandlerTable
		c.attr_16A = 0x1867D86;
		c.attr_184 = 0x1867DA0;
		c.attr_186 = 0x1867DA2;
		c.attr_194 = 0x1867DB0;
		c.attr_19C = 0x1867DB8;
		c.desc = 0x1867954;         // draw_table - 0x2C8
		c.ptr_block = 0x1867A08;    // draw_table - 0x214
		c.queue = 0x27969E8;
		c.file00 = 0x27969FC;
		c.file01 = 0x27969F8;
		c.tick = 0xA48F60;
		c.load_cb = 0xA4D9C0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// engine functions (original addresses)
	// ------------------------------------------------------------------------------------
	// 0x56C850: IR123 = R * v + TR (current GTE matrix, v = 3 x s16) stored as 3 x s16 at out,
	// GTE FLAG -> flag
	static inline void xl_RotTransIR_56C850(const void *v, void *out, void *flag) { x::f<void (__cdecl *)(const void *, void *, void *)>(0x56C850)(v, out, flag); }
	// 0x56C880 TransformWorldCoordinateToProjectedSpace(v, sxy, p, flag): RTPS of v, SXY2 -> sxy,
	// returns OTZ (SZ3 / 4)
	static inline int32_t xl_Project_56C880(const void *v, void *sxy, void *p, void *flag) { return x::f<int32_t (__cdecl *)(const void *, void *, void *, void *)>(0x56C880)(v, sxy, p, flag); }
	// 0x5106E0: draws one object's triangles / quads of a battle model from pre-projected vertices
	// (desc +0 primitive records, +4 projected vertices (SXY, OTZ * 4), +8.. u16 counts, +0x1C
	// back colour) into the OT
	static inline uint32_t xl_RenderProjectedObject_5106E0(void *desc, uint32_t ot, int32_t mode, uint32_t cursor) { return x::f<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x5106E0)(desc, ot, mode, cursor); }
	// 0x5088A0: renders a battle entity model into the render list
	static inline uint32_t xl_Render_5088A0(void *ent, uint32_t ot, int32_t mode, uint32_t cursor) { return x::Render_5088A0(ent, ot, mode, cursor); }

	// ------------------------------------------------------------------------------------
	// 0xA49E90 SyncEntityModelCopy(blk) (= Brothers 0xAF6310): blk+0x10..0x1B = entity+0x60..0x6B
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
	// 0xA49E30 CreateEntityModelCopy(ent) (= Brothers 0xAF62B0): 0x80-byte block from the arena:
	// +0xC = ent, sync (0xA49E90), render descriptor at +0x40: +0x50 = -1, +0x54 = 0,
	// +0x58 = 0xD00140, +0x5C = +0x78 = ent+0x28, +0x60 = ent+0x7C (object mask), +0x64 (u16) = 0,
	// +0x68 = 0; +0 / +2 (u16) = 0, +0x21 (u8) = 0.
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
	// 0xA4A040 ProjectWobbleGroup(s, out, table, m): one vertex group (s16 count, then 3 x s16 per
	// vertex) through the current GTE matrix into 8-byte records at out (IR as 3 x s16), each
	// pushed by the wobble table (x += table[(y >> 2) & 0xFF], z += table[0x200 + same]), then
	// projected with the billboard matrix 0 (identity) and the zero vector: record = SXY, OTZ << 2.
	// ws+0xF8 = the stream after the group, ws+0xFC = the record after the last. (m unused.)
	// ------------------------------------------------------------------------------------
	static void ProjectWobbleGroup(uint8_t *s, uint8_t *out, uint8_t *table)
	{
		uint8_t *rec = out;                                                 // ebx
		int32_t n = S16(s, 0);                                              // [ebp+8]
		s += 2;
		if (n > 0)
		{
			for (int32_t k = n; k > 0; k--)
			{
				U16(WS(), 0xD0) = U16(s, 0);
				U16(WS(), 0xD2) = U16(s, 2);
				U16(WS(), 0xD4) = U16(s, 4);
				uint8_t *w = WS();
				xl_RotTransIR_56C850(w + 0xD0, out, w + 0xFC);
				// mov ax, [out+2] keeps the return value's high half in eax: after sar 2 / and 0xFF
				// only bits 2..9 of the y word remain
				uint32_t i = ((uint32_t)U16(out, 2) >> 2) & 0xFF;
				s += 6;
				out += 8;
				U16(out, -8) = (uint16_t)(U16(out, -8) + U16(table, i * 2));
				U16(out, -4) = (uint16_t)(U16(out, -4) + U16(table, i * 2 + 0x400));
			}
		}
		x::SetRotMatrix(BBMAT());
		x::SetTransVector((const void *)0x27971E4);
		if (n > 0)
		{
			for (int32_t k = n; k > 0; k--)
			{
				uint8_t *flag = WS() + 0xFC;
				int32_t z = xl_Project_56C880(rec, rec, flag, flag);
				S32(rec, 4) = (int32_t)((uint32_t)z << 2);
				rec += 8;
			}
		}
		PTR(WS(), 0xFC) = rec;
		PTR(WS(), 0xF8) = s;
	}

	// ------------------------------------------------------------------------------------
	// 0xA49EE0 DrawWobbleModel(model, desc): model = {+0 skeleton (bone matrices from +0x10,
	// 0x30 each), +4 object table (count, offsets)}; desc = the copy block's render descriptor
	// (+4 = projected vertices at arena top + 0x800, +0x1C.. back colour, +0x20 object mask).
	// ------------------------------------------------------------------------------------
	static void DrawWobbleModel(uint8_t *model, uint8_t *desc)
	{
		PTR(desc, 4) = PTR(CTX(), 0x74) + 0x800;
		uint8_t *mats = PTR(model, 0) + 0x10;                               // [ebp-0xc]
		uint8_t *tab = PTR(model, 4);                                       // edi
		int32_t nobj = S32(tab, 0);                                         // [ebp-0x14]
		tab += 4;
		x::GteSetBackColor3(U8(desc, 0x1C), U8(desc, 0x1D), U8(desc, 0x1E));
		for (int32_t o = 0; o < nobj; o++)
		{
			uint8_t *s = PTR(model, 4) + U32(tab, 0);                       // esi
			tab += 4;
			if (!(U32(desc, 0x20) & (1u << (o & 31)))) continue;
			int32_t ng = S16(s, 0);
			s += 2;
			uint8_t *out = PTR(desc, 4);                                    // [ebp+0xc]
			for (; ng > 0; ng--)
			{
				uint8_t *m = mats + S16(s, 0) * 48 + 0x10;
				x::SetRotMatrix(m);
				x::SetTransVector(m);
				s += 2;
				ProjectWobbleGroup(s, out, PTR(CTX(), 0x74));
				s = PTR(WS(), 0xF8);
				out = PTR(WS(), 0xFC);
			}
			s = (uint8_t *)(((uint32_t)s + 3) & ~3u);
			U16(desc, 8) = U16(s, 0);
			U16(desc, 0xA) = U16(s, 2);
			U16(desc, 0xC) = U16(s, 4);
			U16(desc, 0xE) = U16(s, 6);
			PTR(desc, 0) = s + 0xC;
			U32(CTX(), 0x7C) = xl_RenderProjectedObject_5106E0(desc, U32(RT(), 0x4C), 4, U32(CTX(), 0x7C));
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xA49C20 (Draw 30 WobbleEntity): see the module comment.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_30_WobbleEntity()
	{
		// 30 fps layer: see mag234_fart_held.inc
		FX_HELD(held_entity_begin(PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)));)
		if (U32(CUR(), 0xBC) == 0)
		{
			uint8_t *prm = PTR(CUR(), 0xB8);                                // esi
			PTR(CUR(), 0xC0) = blob::ArenaAlloc(0x10);
			uint8_t *d = PTR(CUR(), 0xC0);
			U16(d, 0) = 0;
			uint16_t fl = U16(prm, 0);
			U16(d, 2) = fl;
			U16(d, 6) = U16(prm, 4);
			if (fl & 1)
				PTR(CUR(), 0xBC) = h_B26860(S16(prm, 2));
			else
			{
				uint8_t *ent = PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B));
				PTR(CUR(), 0xBC) = CreateEntityModelCopy(ent);
				U8(ent, 0) |= 4;
			}
		}
		uint8_t *blk = PTR(CUR(), 0xBC);                                    // eax
		uint8_t *d = PTR(CUR(), 0xC0);                                      // ebx
		uint8_t *ent = PTR(blk, 0xC);                                       // esi
		uint8_t *model = blk + 0x30;                                        // [ebp-0x14]
		uint8_t *desc = blk + 0x40;                                         // [ebp-0x10]
		uint8_t *hdr = blk + 0x10;                                          // edi
		uint8_t *weapon = nullptr;                                          // [ebp-4]
		if (U8(d, 2) & 1)
		{
			h_B269E0();
			h_B26AD0(PTR(CUR(), 0xBC));
		}
		else
		{
			SyncEntityModelCopy(blk);
			weapon = PTR(ent, 0x78);
			// 30 fps layer: see mag234_fart_held.inc
			FX_HELD(held_local_skeletons(hdr, weapon);)
			x::ComposeAffine(NODEMAT(), ent + 0x40, WS() + 0xE0);
			if (weapon) x::ComputeBonesWorldMatrices(weapon, WS() + 0xE0);
			x::ComputeBonesWorldMatrices(hdr, WS() + 0xE0);
			U32(CTX(), 0x7C) = xl_Render_5088A0(ent, FRAMERL() + 0x4064, 0x1F, U32(CTX(), 0x7C));
		}
		// 0xA49D97: the wobble table
		uint8_t *tab = PTR(CTX(), 0x74);                                    // edi
		uint8_t *node = blob::GetBone(S16(d, 6));
		int32_t step = S16(node, 0x98);                                     // [ebp-0xc]
		int32_t amp = S16(node, 0x94);                                      // ebx
		int32_t phase = S16(node, 0x96);                                    // esi
		for (int32_t k = 0x100; k != 0; k--)
		{
			U16(tab, 0) = (uint16_t)(mul32(x::Sin(phase), amp) >> 12);
			U16(tab, 0x400) = (uint16_t)(mul32(x::Cos(phase), amp) >> 12);
			phase = add32(phase, step);
			tab += 2;
		}
		DrawWobbleModel(model, desc);
		if (weapon) DrawWobbleModel(PTR(weapon, 4), desc);
		// 30 fps layer: see mag234_fart_held.inc
		FX_HELD(held_entity_end();)
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag234_fart()
	{
		using namespace gfc;
		fart::describe(fart::g_c);
		init_clone(fart::g_c, nullptr, 0);
		fart::g_c.draw[30] = fart::dh_30_WobbleEntity;
		register_port(0xA48F60, (void *)fart::SequenceTask, "MAG234 Fart SequenceTick", 234);
		// 30 fps layer: see mag234_fart_held.inc
		FX_HELD(register_mag234_fart_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag234_fart_held.inc"
#endif
