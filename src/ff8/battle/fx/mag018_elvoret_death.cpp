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

// Effect 18: Elvoret's death (Dollet communication tower, scene encounter 29). No kernel.bin entry
// uses effect 18: Elvoret c0m091's AI death script launches it itself, `giveItem(...); target(SELF);
// prepareAnim(16); attackAnimation(18); vanish()` (AI opcode 0x1E at 0x489A28: command 0xFC, the
// effect id byte through DRAW_TARGET_ID 0x1D27ADA -> 0x1D27ADC (Battle_applyDamage 0x4905E4) -> the
// action's effect id -> BattleActionSequence_Tick_MagicEffect 0x50B190); the caster and the only
// target are Elvoret's own slot. Cinematic-engine module MAG_018 0xB4A500..0xB58050 (the tail of
// .text, just before Leviathan's 0xB58050), script data magic/mag017_b.00 / .01 (loaded by
// MAG_018_ELVORET_DEATH_FL 0xB4A500; setup MAG_018_ELVORET_DEATH 0xB4A530 = effect table index 17).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with Ifrit's lit
// dispatcher: instruction-level comparison against the ported clones (module-relative addresses
// normalised, table regions symbolic, jump tables and callee trees hashed, w/gfc/cmp018.py):
//   - SetupSummon 0xB4A590, BindContext 0xB4A5E0, the section pointers 0xB4A680, SequenceTick
//     0xB4ABA0 (BuildMatricesAndDraw 0xB52890, lit), the anim channels / integrator and
//     InitSequenceState 0xB57660 are byte clones of Ifrit's; SetInitialFrameLimit 0xB58030 of
//     Leviathan's; lit_dispatcher true;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub, except
//       VM 0x006 LoadBattleFile 0xB53720: same code, own load callback 0xB53850 (c.load_cb),
//       draw 16 0xB4B450 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU, prim 16 0xB50540
//         = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4, VM 0x042 0xB57280 = Bahamut 0xB24BB0 /
//         Breath of Death 0xA658D0 EntityChainAnim (apply_shared_misc, only those slots taken),
//       draw 22 0xB4BBE0 EntityDisintegrate = Final Form Death's 0xB322E0 and its 22 callees
//         0xB4BFA0..0xB4E770, byte for byte but for the four depth-key cells (0x2796EE8..0x2796EF4
//         here, 0x2796E70..0x2796E7C there): ported below (the same code as
//         mag077_final_form_death.cpp, its keys, its own 30 fps part),
//       VM 0x0AF 0xB52F20 SetInlineDescriptor (+ 0xB52F50) = Final Form Death's 0xB39820: not
//         reached by this script either (coverage over every draw-22 mode): kept original, as there.
// The effect's queue (0x2796EC0) holds one task, the engine tick 0xB4ABA0.
// Battle entity writes: VM 0x042 queues the caster's chain transformation 0x1E (0x505C00); draw 22
// sets the slot entity's flag +0 |= 4 on its first draw (the real model hidden: the copy falls
// apart) and rebuilds the entity skeleton (pose + world matrices) on every draw. No entity
// position (+0x1C..+0x20) is written. The battle camera is written by the engine's VM 0x039
// (stage camera animation, eye / look-at 0xB8B7F0 / 0xB8B7F8): the 30 fps part's HeldCamera.
// Other side effects: sounds (BdPlaySE 0x501330, voice slot 0x4A29A0 / 0x4A2940), the music
// command 0x47E3C0, VRAM uploads 0x505DF0 (draw 16's texture scroll).
//
// Draw 22 EntityDisintegrate: see mag077_final_form_death.cpp (the same code): the slot entity's
// model copy falls apart polygon by polygon (pieces, dots, particles).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace elvoret018
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag018_elvoret_death_held.h"
#endif

namespace ff8fx
{
namespace gfc
{

namespace elvoret018
{
	static void describe(Clone &c)
	{
		c.name = "018 Elvoret Death";
		c.effect_id = 18;
		c.vm_table = 0x1876D34;     // MAG_018 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x18769A4;   // MAG_018 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x18769EC;   // bone_table + 0x48
		c.prim_size = 0x1876ADC;    // bone_table + 0x138
		c.spr_table = 0x1876AF0;    // bone_table + 0x14C
		c.ptype_table = 0x1876B1C;  // bone_table + 0x178
		c.pop_table = 0x1876B30;    // bone_table + 0x18C
		c.draw_table = 0x1876B90;   // MAG_018 DrawHandlerTable
		c.attr_16A = 0x1876CFA;
		c.attr_184 = 0x1876D14;
		c.attr_186 = 0x1876D16;
		c.attr_194 = 0x1876D24;
		c.attr_19C = 0x1876D2C;
		c.desc = 0x1876870;         // draw_table - 0x320 (read by 0xB4A680)
		c.ptr_block = 0x187697C;    // draw_table - 0x214 (read by 0xB4A5E0)
		c.queue = 0x2796EC0;
		c.file00 = 0x2796ED4;
		c.file01 = 0x2796ED0;
		c.tick = 0xB4ABA0;
		c.load_cb = 0xB53850;
		c.lit_dispatcher = true;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// engine calls not wrapped by gfc_engine.h
	// ------------------------------------------------------------------------------------
	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m; joint origin -> ws+0xF0..0xF8. Returns ws+0xD0. Writes only ws + GTE.
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m);
	}
	// 0x56C850 RotTrans: LoadV0(v); MVMVA(R*V0+TR); IR1..3 -> out (3 x s16); FLAG -> *flag
	static inline void xl_RotTrans(const void *v, void *out, void *flag) { x::f<void (__cdecl *)(const void *, void *, void *)>(0x56C850)(v, out, flag); }
	// 0x56C880 TransformWorldCoordinateToProjectedSpace (RotTransPers): returns OTZ
	static inline int32_t xl_RotTransPers(const void *v, void *sxy, void *p, void *flag) { return x::f<int32_t (__cdecl *)(const void *, void *, void *, void *)>(0x56C880)(v, sxy, p, flag); }
	// 0x50FD40: projects one vertex group of a battle model (count + 3 x s16 vertices at *stream,
	// through the current GTE matrix) to 8-byte records (SXY, depth, flags) at *out; advances both
	static inline void xl_ProjectVertexGroup_50FD40(uint8_t **stream, uint8_t **out) { x::f<void (__cdecl *)(uint8_t **, uint8_t **)>(0x50FD40)(stream, out); }
	// x / 3 as compiled (imul 0x55555556, + sign bit of the high half)
	static inline int32_t div3(int32_t v)
	{
		int32_t hi = (int32_t)(((int64_t)v * 0x55555556LL) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// depth keys of the module's inserts (0x2796EE8..0x2796EF4, engine state block)
	inline int32_t &KEY70() { return MEM<int32_t>(0x2796EE8); }
	inline int32_t &KEY74() { return MEM<int32_t>(0x2796EEC); }
	inline int32_t &KEY78() { return MEM<int32_t>(0x2796EF0); }
	inline int32_t &KEY7C() { return MEM<int32_t>(0x2796EF4); }

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0xB4C4E0 SyncEntityModelCopy(blk) (= Brothers 0xAF6310): blk+0x10..0x1B = entity+0x60..0x6B
	// (BattleAnimHeader: +0x14 = the entity's model pointer P), blk+0x30..0x3F = the 16 bytes at P
	// (entity = blk+0xC).
	// ------------------------------------------------------------------------------------
	static void SyncEntityModelCopy_B4C4E0(uint8_t *blk)
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
	// 0xB4C480 CreateEntityModelCopy(ent) (= Brothers 0xAF62B0): 0x80-byte block from the arena:
	// +0xC = ent, sync, render descriptor at +0x40 (+0x50 = -1, +0x54 = 0, +0x58 = 0xD00140,
	// +0x5C = +0x78 = ent+0x28, +0x60 = ent+0x7C, +0x64 = 0, +0x68 = 0), +0 / +2 = 0, +0x21 = 0.
	// ------------------------------------------------------------------------------------
	static uint8_t *CreateEntityModelCopy_B4C480(uint8_t *ent)
	{
		uint8_t *blk = blob::ArenaAlloc(0x80);
		PTR(blk, 0xC) = ent;
		SyncEntityModelCopy_B4C4E0(blk);
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
	// 0xB4C090 CountModelPolygons(mdl = copy+0x30): ws+0xFC / ws+0xF8 = the G3 / G4 counts of
	// every object (s16 pair after the vertex groups, 4-aligned). Returns the vertex count.
	// ------------------------------------------------------------------------------------
	static int32_t CountModelPolygons_B4C090(uint8_t *mdl)
	{
		S32(WS(), 0xFC) = 0;
		S32(WS(), 0xF8) = 0;
		int32_t nvert = 0;
		uint8_t *tab = PTR(mdl, 4);                                           // esi
		int32_t n = S32(tab, 0);
		tab += 4;
		for (; n > 0; n--)
		{
			uint8_t *o = PTR(mdl, 4) + U32(tab, 0);                           // ecx
			tab += 4;
			int32_t ng = S16(o, 0);
			o += 2;
			for (; ng > 0; ng--)
			{
				int32_t nv = S16(o, 2);
				o += 2;
				nvert += nv;
				o += nv * 6 + 2;
			}
			o = (uint8_t *)(((uint32_t)o + 3) & ~3u);
			S32(WS(), 0xFC) = add32(S32(WS(), 0xFC), S16(o, 0));
			S32(WS(), 0xF8) = add32(S32(WS(), 0xF8), S16(o, 2));
		}
		return nvert;
	}

	// 0xB4CBD0 / 0xB4CC20: sum of the camera-space Y (scratch ctx+0x74, 8 bytes per vertex) of a
	// G3 / G4 record's vertices
	static int32_t SumY3_B4CBD0(const uint8_t *prim)
	{
		const uint8_t *v = PTR(CTX(), 0x74);
		return S16(v, 8 * (U16(prim, 4) & 0xFFF) + 2) + S16(v, 8 * (U16(prim, 2) & 0xFFF) + 2) + S16(v, 8 * (U16(prim, 0) & 0xFFF) + 2);
	}
	static int32_t SumY4_B4CC20(const uint8_t *prim)
	{
		const uint8_t *v = PTR(CTX(), 0x74);
		int32_t s = S16(v, 8 * (U16(prim, 6) & 0xFFF) + 2) + S16(v, 8 * (U16(prim, 4) & 0xFFF) + 2);
		return s + S16(v, 8 * (U16(prim, 2) & 0xFFF) + 2) + S16(v, 8 * (U16(prim, 0) & 0xFFF) + 2);
	}

	// ------------------------------------------------------------------------------------
	// 0xB4D2D0 PieceVelocity(desc, p): random velocity / acceleration X, Y, Z (base + Rand73(range))
	// ------------------------------------------------------------------------------------
	static void PieceVelocity_B4D2D0(uint8_t *desc, uint8_t *p)
	{
		U16(p, 0x3C) = (uint16_t)(U16(desc, 0x12) + blob::Rand73(S16(desc, 0x14)));
		U16(p, 0x3E) = (uint16_t)(U16(desc, 0x16) + blob::Rand73(S16(desc, 0x18)));
		U16(p, 0x16) = (uint16_t)(U16(desc, 0x1A) + blob::Rand73(S16(desc, 0x1C)));
		U16(p, 0x1E) = (uint16_t)(U16(desc, 0x1E) + blob::Rand73(S16(desc, 0x20)));
		U16(p, 0x40) = (uint16_t)(U16(desc, 0x22) + blob::Rand73(S16(desc, 0x24)));
		U16(p, 0x42) = (uint16_t)(U16(desc, 0x26) + blob::Rand73(S16(desc, 0x28)));
	}

	// ------------------------------------------------------------------------------------
	// 0xB4D3F0 SpawnDot(p): the next dot slot (piece block +0x1C counter mod desc+0x2E) at the
	// piece's centroid, life desc+0x32, random velocity / acceleration; mode 6: position as
	// radius (blob distance, x256) / angle (x16).
	// ------------------------------------------------------------------------------------
	static void SpawnDot_B4D3F0(uint8_t *p)
	{
		uint8_t *st = PTR(WS(), 0x84);                                        // ecx
		uint8_t *desc = PTR(WS(), 0x90);                                      // esi
		int32_t c = U16(st, 0x1C);
		U16(st, 0x1C) = (uint16_t)(c + 1);
		int32_t idx = c % (int32_t)U16(desc, 0x2E);
		uint8_t *d = PTR(WS(), 0x8C) + idx * 0x1C;                            // edi
		U8(d, 0x18) = 1;
		U8(d, 0x19) = U8(desc, 0x32);
		U32(d, 0) = U32(p, 4);
		U32(d, 4) = U32(p, 8);
		U32(d, 8) = U32(p, 0xC);
		U16(d, 0xC) = (uint16_t)(U16(desc, 0x34) + blob::Rand73(S16(desc, 0x36)));
		U16(d, 0xE) = (uint16_t)(U16(desc, 0x38) + blob::Rand73(S16(desc, 0x3A)));
		U16(d, 0x10) = (uint16_t)(U16(desc, 0x3C) + blob::Rand73(S16(desc, 0x3E)));
		U16(d, 0x12) = (uint16_t)(U16(desc, 0x40) + blob::Rand73(S16(desc, 0x42)));
		U16(d, 0x14) = (uint16_t)(U16(desc, 0x44) + blob::Rand73(S16(desc, 0x46)));
		U16(d, 0x16) = (uint16_t)(U16(desc, 0x48) + blob::Rand73(S16(desc, 0x4A)));
		if (U16(desc, 8) == 6)
		{
			int32_t a = blob::Blob_B66B30(0, 0, S32(d, 0) >> 8, S32(d, 8) >> 8);
			S32(d, 0) = shl32(S32(WS(), 0xFC), 8);
			S32(d, 8) = shl32(a, 4);
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB4D510 SpawnParticle(p): with chance desc+0x10 / 256 (Rand74(0x100) <= it), one record of
	// the particle pool of bone desc+0xE (draw 6 pool at that bone+0xB8: count +0, counter +2,
	// 0x50-byte records from +0x10) at the piece's centroid.
	// ------------------------------------------------------------------------------------
	static void SpawnParticle_B4D510(uint8_t *p)
	{
		uint8_t *desc = PTR(WS(), 0x90);                                      // ebx
		uint8_t *st = PTR(WS(), 0x84);                                        // edi
		int32_t id = S16(desc, 0xE);
		if (id == 0) return;
		if (blob::Rand74(0x100) > S16(desc, 0x10)) return;
		uint8_t *b = blob::GetBone(id);                                       // esi
		uint8_t *pool = PTR(b, 0xB8);                                         // ecx
		int32_t k = U16(pool, 2);
		U16(pool, 2) = (uint16_t)(k + 1);
		int32_t idx = k % (int32_t)U16(pool, 0);
		uint8_t *r = pool + idx * 0x50 + 0x10;                                // eax
		U16(pool, 8) = 1;
		U16(r, 0) = 1;
		U16(r, 2) = 0;
		U32(r, 4) = U32(st, 0x10);
		U32(r, 0x14) = U32(p, 4);
		U32(r, 0x18) = U32(p, 8);
		U32(r, 0x1C) = U32(p, 0xC);
		U32(r, 0x20) = 0;
		U32(r, 0x24) = 0;
		U32(r, 0x28) = 0;
		U32(r, 0x2C) = 0;
		U16(r, 0x40) = U16(b, 0x92);
		U16(r, 0x48) = U16(b, 0x9A);
		U32(r, 0x3C) = 0;
		U32(r, 0x38) = U32(b, 0xCC) & 0x2FFFFFF;
		U8(r, 0x37) = 1;
	}

	// ------------------------------------------------------------------------------------
	// 0xB4CC80 / 0xB4CF60 LaunchPiece3 / LaunchPiece4(p, prim, stp): the polygon becomes piece p:
	// state byte 1, life desc+0xC, random motion (0xB4D2D0), centroid (x256) of its camera-space
	// vertices (scratch ctx+0x74) and the vertex offsets from it, spin angle = blob 0xB66B30
	// angle of the centroid (distance -> +0x36), spin speed desc+0x2A + Rand73(desc+0x2C),
	// desc+0x30 dots, maybe a particle.
	// ------------------------------------------------------------------------------------
	static void LaunchPiece3_B4CC80(uint8_t *p, uint8_t *prim, uint8_t *stp)
	{
		uint8_t *desc = PTR(WS(), 0x90);                                      // edi
		U8(p, 0x26) = 0;
		PTR(p, 0) = prim;
		U8(stp, 0) = 1;
		U16(p, 0x30) = (uint16_t)(uint32_t)(stp - PTR(WS(), 0x88));
		U8(p, 0x2E) = U8(desc, 0xC);
		PieceVelocity_B4D2D0(desc, p);
		uint8_t *scr = PTR(CTX(), 0x74);                                      // eax
		static const int vo[3] = { 0xC0, 0xC8, 0xD0 };
		for (int k = 0; k < 3; k++)
		{
			uint8_t *v = scr + 8 * (U16(prim, 2 * k) & 0xFFF);
			U16(WS(), vo[k]) = U16(v, 0);
			U16(WS(), vo[k] + 2) = U16(v, 2);
			U16(WS(), vo[k] + 4) = U16(v, 4);
		}
		for (int a = 0; a < 3; a++)   // x, y, z
		{
			uint8_t *w = WS();
			int32_t c = div3(S16(w, 0xD0 + 2 * a) + S16(w, 0xC8 + 2 * a) + S16(w, 0xC0 + 2 * a));
			S32(p, 4 + 4 * a) = shl32(c, 8);
			U16(p, 0x10 + 2 * a) = (uint16_t)(U16(WS(), 0xC0 + 2 * a) - (uint16_t)c);
			U16(p, 0x18 + 2 * a) = (uint16_t)(U16(WS(), 0xC8 + 2 * a) - (uint16_t)c);
			U16(p, 0x20 + 2 * a) = (uint16_t)(U16(WS(), 0xD0 + 2 * a) - (uint16_t)c);
		}
		U16(p, 0x32) = (uint16_t)blob::Blob_B66B30(0, 0, S32(p, 4) >> 8, S32(p, 0xC) >> 8);
		U16(p, 0x36) = U16(WS(), 0xFC);
		U16(p, 0x34) = (uint16_t)(U16(desc, 0x2A) + blob::Rand73(S16(desc, 0x2C)));
		for (int32_t n = U16(desc, 0x30); n > 0; n--) SpawnDot_B4D3F0(p);
		SpawnParticle_B4D510(p);
	}

	static void LaunchPiece4_B4CF60(uint8_t *p, uint8_t *prim, uint8_t *stp)
	{
		uint8_t *desc = PTR(WS(), 0x90);                                      // edi
		U8(p, 0x26) = 1;
		PTR(p, 0) = prim;
		U8(stp, 0) = 1;
		U16(p, 0x30) = (uint16_t)(uint32_t)(stp - PTR(WS(), 0x88));
		U8(p, 0x2E) = U8(desc, 0xC);
		PieceVelocity_B4D2D0(desc, p);
		uint8_t *scr = PTR(CTX(), 0x74);                                      // eax
		static const int vo[4] = { 0xC0, 0xC8, 0xD0, 0xD8 };
		for (int k = 0; k < 4; k++)
		{
			uint8_t *v = scr + 8 * (U16(prim, 2 * k) & 0xFFF);
			U16(WS(), vo[k]) = U16(v, 0);
			U16(WS(), vo[k] + 2) = U16(v, 2);
			U16(WS(), vo[k] + 4) = U16(v, 4);
		}
		for (int a = 0; a < 3; a++)   // x, y, z
		{
			uint8_t *w = WS();
			int32_t s = S16(w, 0xD8 + 2 * a) + S16(w, 0xD0 + 2 * a);
			s += S16(w, 0xC8 + 2 * a);
			s += S16(w, 0xC0 + 2 * a);
			int32_t c = s / 4;
			S32(p, 4 + 4 * a) = shl32(c, 8);
			U16(p, 0x10 + 2 * a) = (uint16_t)(U16(WS(), 0xC0 + 2 * a) - (uint16_t)c);
			U16(p, 0x18 + 2 * a) = (uint16_t)(U16(WS(), 0xC8 + 2 * a) - (uint16_t)c);
			U16(p, 0x20 + 2 * a) = (uint16_t)(U16(WS(), 0xD0 + 2 * a) - (uint16_t)c);
			U16(p, 0x28 + 2 * a) = (uint16_t)(U16(WS(), 0xD8 + 2 * a) - (uint16_t)c);
		}
		U16(p, 0x32) = (uint16_t)blob::Blob_B66B30(0, 0, S32(p, 4) >> 8, S32(p, 0xC) >> 8);
		U16(p, 0x36) = U16(WS(), 0xFC);
		U16(p, 0x34) = (uint16_t)(U16(desc, 0x2A) + blob::Rand73(S16(desc, 0x2C)));
		for (int32_t n = U16(desc, 0x30); n > 0; n--) SpawnDot_B4D3F0(p);
		SpawnParticle_B4D510(p);
	}

	// ------------------------------------------------------------------------------------
	// 0xB4D360 LaunchByCounter(p, prim, stp, quad): with dots (desc+0x2E), every ws+0x40-th call
	// (countdown ws+0x44) launches the polygon (0xB4CC80 / 0xB4CF60) plus desc+0x30 more dots;
	// returns 1 then, else 0.
	// ------------------------------------------------------------------------------------
	static int32_t LaunchByCounter_B4D360(uint8_t *p, uint8_t *prim, uint8_t *stp, int32_t quad)
	{
		uint8_t *desc = PTR(WS(), 0x90);                                      // esi
		if (U16(desc, 0x2E) == 0) return 0;
		S32(WS(), 0x44) = S32(WS(), 0x44) - 1;
		if (S32(WS(), 0x44) > 0) return 0;
		S32(WS(), 0x44) = S32(WS(), 0x40);
		if (quad == 0) LaunchPiece3_B4CC80(p, prim, stp);
		else LaunchPiece4_B4CF60(p, prim, stp);
		for (int32_t n = U16(desc, 0x30); n > 0; n--) SpawnDot_B4D3F0(p);
		return 1;
	}

	// the first free piece slot from p (the slots are not bounded here: the caller checks the
	// alive count against the slot count)
	static inline uint8_t *FreeSlot(uint8_t *p)
	{
		while (U32(p, 0) != 0) p += 0x44;
		return p;
	}

	// ------------------------------------------------------------------------------------
	// 0xB4C530 UpdatePolygons: for every object of the model copy: its vertices transformed to
	// camera space (0x56C850, scratch ctx+0x74), then by mode (ws+0x50 = desc+8):
	//   0-3 / 6: a polygon still on the model whose vertex Y sum is below 3 x (4 x) outPos Y is
	//            launched; otherwise the counter may launch it (0xB4D360)
	//   7:       the same with the test reversed (Y sum above)
	//   5:       the test is the per-polygon value (piece block +0x18, s16 +2 per polygon) above
	//            outPos Y (ws+0xF0 -> bone+0x96 here)
	//   4:       nothing.
	// A launch needs a free slot (alive count +6 < slots +2 + 1, else the update ends).
	// VANILLA QUIRK 0xB4CB97: the exit stores the local alive count ([ebp-4]) to +6; with mode 4
	// (or a model without objects) that local is never written: its slot is the one that held the
	// return address 0xB4BE7E of the caller's call to 0x508C90 at the same stack depth just before
	// (draw 22 calls nothing else in between), so +6 = 0xBE7E (return address 0xB4BE7E here; the mode comes from
	// the script's draw-22 descriptor, see README017).
	// ------------------------------------------------------------------------------------
	static void UpdatePolygons_B4C530()
	{
		S32(WS(), 0xE4) = S32(WS(), 0xF0);
		uint8_t *stp = PTR(WS(), 0x88);                                       // [ebp-8]
		uint8_t *st = PTR(WS(), 0x84);                                        // [ebp-0x10]
		uint8_t *per = PTR(st, 0x18);                                         // [ebp-0x14]
		uint8_t *mdl = PTR(WS(), 0x80) + 0x30;                                // [ebp-0x20]
		uint8_t *bones = PTR(mdl, 0) + 0x10;                                  // [ebp-0x28]
		uint8_t *tab = PTR(mdl, 4);
		int32_t nobj = S32(tab, 0);                                           // [ebp-0x2c]
		tab += 4;
		int32_t alive = 0;                                                    // [ebp-4] (see the quirk)
		bool alive_set = false;
		for (int32_t o = 0; o < nobj; o++)
		{
			uint8_t *s = PTR(mdl, 4) + U32(tab, 0);                           // esi
			tab += 4;
			uint8_t *out = PTR(CTX(), 0x74);                                  // ebx
			int32_t ng = S16(s, 0);
			s += 2;
			uint8_t *v = WS() + 0xF0;                                         // edi
			for (; ng > 0; ng--)
			{
				uint8_t *m = bones + S16(s, 0) * 48 + 0x10;
				s += 2;
				x::SetRotMatrix(m);
				x::SetTransVector(m);
				int32_t nv = S16(s, 0);
				s += 2;
				for (; nv > 0; nv--)
				{
					U16(v, 0) = U16(s, 0);
					U16(v, 2) = U16(s, 2);
					U16(v, 4) = U16(s, 4);
					xl_RotTrans(v, out, WS() + 0xFC);
					out += 8;
					s += 6;
				}
			}
			uint8_t *c = (uint8_t *)(((uint32_t)s + 3) & ~3u);
			S32(WS(), 0xE8) = S16(c, 0);
			c += 2;
			S32(WS(), 0xEC) = S16(c, 0);
			c += 0xA;
			const uint32_t mode = U32(WS(), 0x50);
			if (mode > 7 || mode == 4) continue;
			uint8_t *prim = c;                                                // edi
			st = PTR(WS(), 0x84);
			uint8_t *p = st + 0x20;                                           // esi
			S32(WS(), 0xE0) = (int32_t)U16(st, 2) + 1;
			alive = U16(st, 6);
			alive_set = true;
			// ---- G3 ----
			{
				int32_t lim = mode == 5 ? (int32_t)S16(CUR(), 0x96) : S32(WS(), 0xE4) * 3;   // [ebp-0xc]
				for (int32_t n = S32(WS(), 0xE8); n > 0; n--)
				{
					if (U8(stp, 0) == 0)
					{
						bool take;
						if (mode == 5) take = S16(per, 2) > lim;
						else if (mode == 7) take = SumY3_B4CBD0(prim) > lim;
						else take = SumY3_B4CBD0(prim) < lim;
						if (alive >= S32(WS(), 0xE0)) goto done;
						p = FreeSlot(p);
						if (take)
						{
							LaunchPiece3_B4CC80(p, prim, stp);
							p += 0x44;
							alive++;
						}
						else if (LaunchByCounter_B4D360(p, prim, stp, 0) == 1)
						{
							p += 0x44;
							alive++;
						}
					}
					stp++;
					if (mode == 5) per += 4;
					prim += 0x10;
				}
			}
			U16(st, 6) = (uint16_t)alive;
			// ---- G4 ----
			{
				int32_t lim = mode == 5 ? (int32_t)S16(CUR(), 0x96) : shl32(S32(WS(), 0xE4), 2);
				for (int32_t n = S32(WS(), 0xEC); n > 0; n--)
				{
					if (U8(stp, 0) == 0)
					{
						bool take;
						if (mode == 5) take = S16(per, 2) > lim;
						else if (mode == 7) take = SumY4_B4CC20(prim) > lim;
						else take = SumY4_B4CC20(prim) < lim;
						if (alive >= S32(WS(), 0xE0)) goto done;
						p = FreeSlot(p);
						if (take)
						{
							LaunchPiece4_B4CF60(p, prim, stp);
							p += 0x44;
							alive++;
						}
						else if (LaunchByCounter_B4D360(p, prim, stp, 1) == 1)
						{
							p += 0x44;
							alive++;
						}
					}
					stp++;
					if (mode == 5) per += 4;
					prim += 0x14;
				}
			}
			U16(st, 6) = (uint16_t)alive;
		}
	done:
		// alive_set false: the never-written stack word, see above
		U16(st, 6) = alive_set ? (uint16_t)alive : (uint16_t)0xBE7E;
	}

	static void InsertPrim_B4DDD0(int32_t z, void *prim);

	// FixedPointCrossProduct 0x56BBF0(a, b, out) / Math_NormalizeVec3_Q12 0x56BD20(in, out) /
	// 0x56CB30 NCLIP of three SXY words (result unused by the module, GTE side effects) /
	// 0x56CA30 colour of a normal (ws+0xFC <- lit colour)
	static inline void xl_Cross_56BBF0(const void *a, const void *b, void *out) { x::f<void (__cdecl *)(const void *, const void *, void *)>(0x56BBF0)(a, b, out); }
	static inline void xl_Normalize_56BD20(const void *in, void *out) { x::f<void (__cdecl *)(const void *, void *)>(0x56BD20)(in, out); }
	static inline int32_t xl_NClip_56CB30(uint32_t s0, uint32_t s1, uint32_t s2) { return x::f<int32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56CB30)(s0, s1, s2); }
	static inline void xl_NormalColor_56CA30(const void *n, const void *col, void *out) { x::f<void (__cdecl *)(const void *, const void *, void *)>(0x56CA30)(n, col, out); }

	// ------------------------------------------------------------------------------------
	// 0xB4D5F0 PolygonValues(hdr, out) (mode 5, first draw): the model posed (0x508C90), joint
	// 0xF1's origin (blob 0xB66B80 with the billboard matrix 0x2797968) -> ws+0xD0..0xD4; every
	// vertex in camera space (scratch ctx+0x74); per G3 / G4: blob 0xB66B30(joint X, joint Z,
	// centroid X, centroid Z) -> u16 +0, ws+0xFC -> u16 +2 (4 bytes per polygon).
	// ------------------------------------------------------------------------------------
	static void PolygonValues_B4D5F0(uint8_t *hdr, uint8_t *out)
	{
		x::BuildBoneMatricesFromPose(hdr);
		xl_JointMatrix_B66B80(hdr, 0xF1, 0x1000, BBMAT());
		uint8_t *w = WS();
		U16(w, 0xD0) = U16(w, 0xF0);
		U16(WS(), 0xD2) = U16(WS(), 0xF4);
		U16(WS(), 0xD4) = U16(WS(), 0xF8);
		uint8_t *mdl = PTR(hdr, 4);                                           // [ebp-0xc]
		uint8_t *bones = PTR(mdl, 0) + 0x10;                                  // [ebp-0x14]
		uint8_t *tab = PTR(mdl, 4);
		int32_t n = S32(tab, 0);
		tab += 4;
		for (; n > 0; n--)
		{
			uint8_t *s = PTR(mdl, 4) + U32(tab, 0);                           // esi
			tab += 4;
			uint8_t *scr = PTR(CTX(), 0x74);                                  // [ebp+8]
			int32_t ng = S16(s, 0);
			s += 2;
			uint8_t *v = WS() + 0xF0;                                         // ebx
			for (; ng > 0; ng--)
			{
				uint8_t *m = bones + S16(s, 0) * 48 + 0x10;
				s += 2;
				x::SetRotMatrix(m);
				x::SetTransVector(m);
				int32_t nv = S16(s, 0);
				s += 2;
				for (; nv > 0; nv--)
				{
					U16(v, 0) = U16(s, 0);
					U16(v, 2) = U16(s, 2);
					U16(v, 4) = U16(s, 4);
					xl_RotTrans(v, scr, WS() + 0xFC);
					scr += 8;
					s += 6;
				}
			}
			uint8_t *q = (uint8_t *)(((uint32_t)s + 3) & ~3u);               // ebx
			S32(WS(), 0xE8) = S16(q, 0);
			q += 2;
			S32(WS(), 0xEC) = S16(q, 0);
			q += 0xA;
			const uint8_t *sv = PTR(CTX(), 0x74);                             // esi
			while (S32(WS(), 0xE8) > 0)
			{
				const uint8_t *v0 = sv + 8 * (U16(q, 0) & 0xFFF), *v1 = sv + 8 * (U16(q, 2) & 0xFFF), *v2 = sv + 8 * (U16(q, 4) & 0xFFF);
				const int32_t cx = div3(S16(v0, 0) + S16(v1, 0) + S16(v2, 0));
				const int32_t cz = div3(S16(v0, 4) + S16(v1, 4) + S16(v2, 4));
				w = WS();
				U16(out, 0) = (uint16_t)blob::Blob_B66B30(S16(w, 0xD0), S16(w, 0xD4), cx, cz);
				out += 4;
				q += 0x10;
				U16(out, -2) = U16(WS(), 0xFC);
				S32(WS(), 0xE8) = S32(WS(), 0xE8) - 1;
			}
			if (S32(WS(), 0xEC) > 0)
			{
				q += 4;
				while (S32(WS(), 0xEC) > 0)
				{
					const uint8_t *v0 = sv + 8 * (U16(q, -4) & 0xFFF), *v1 = sv + 8 * (U16(q, -2) & 0xFFF);
					const uint8_t *v2 = sv + 8 * (U16(q, 0) & 0xFFF), *v3 = sv + 8 * (U16(q, 2) & 0xFFF);
					const int32_t cx = (S16(v0, 0) + S16(v1, 0) + S16(v2, 0) + S16(v3, 0)) / 4;
					const int32_t cz = (S16(v0, 4) + S16(v1, 4) + S16(v2, 4) + S16(v3, 4)) / 4;
					w = WS();
					U16(out, 0) = (uint16_t)blob::Blob_B66B30(S16(w, 0xD0), S16(w, 0xD4), cx, cz);
					out += 4;
					q += 0x14;
					U16(out, -2) = U16(WS(), 0xFC);
					S32(WS(), 0xEC) = S32(WS(), 0xEC) - 1;
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB4C130 PolygonNormals(hdr, out) (bone+0xE1, first draw): the model posed (0x508C90), its
	// polygon counts (0xB4C090 -> ws+0xF8 / 0xFC), every vertex rotated by its bone (GTE MVMVA,
	// MAC1..3) into the scratch after the camera-space vertices (ctx+0x74 + 8 x vertex count);
	// per G3 / G4: the normalised cross product of (v1 - v0) x (v2 - v0) -> out (8 bytes each).
	// ------------------------------------------------------------------------------------
	static void PolygonNormals_B4C130(uint8_t *hdr, uint8_t *out)
	{
		x::BuildBoneMatricesFromPose(hdr);
		uint8_t *mdl = PTR(hdr, 4);                                           // esi / [ebp-0xc]
		const int32_t nvert = CountModelPolygons_B4C090(mdl);
		uint8_t *vbase = PTR(CTX(), 0x74) + 8 * nvert;                        // edi / [ebp-0x18]
		uint8_t *tab = PTR(mdl, 4) + 4;                                       // ecx
		uint8_t *bones = PTR(mdl, 0) + 0x10;                                  // [ebp-0x14]
		int32_t n = S32(tab, -4);
		for (; n > 0; n--)
		{
			uint8_t *s = PTR(mdl, 4) + U32(tab, 0);                           // esi
			tab += 4;
			uint8_t *o = vbase;                                               // ebx
			int32_t ng = S16(s, 0);
			s += 2;
			for (; ng > 0; ng--)
			{
				uint8_t *m = bones + S16(s, 0) * 48 + 0x10;
				s += 2;
				x::SetRotMatrix(m);
				x::SetTransVector(m);
				int32_t nv = S16(s, 0);
				s += 2;
				for (; nv > 0; nv--)
				{
					x::GteWriteData((int32_t)(((uint32_t)U16(s, 2) << 16) | U16(s, 0)), 0);
					x::GteWriteData((int32_t)U16(s, 4), 1);
					x::GteMVMVA_RotV0Tr();
					U16(o, 0) = MEM<uint16_t>(0x1CA8A74);
					U16(o, 2) = (uint16_t)MEM<uint32_t>(0x1CA8A78);
					U16(o, 4) = (uint16_t)MEM<uint32_t>(0x1CA8A7C);
					o += 8;
					s += 6;
				}
			}
			s = (uint8_t *)(((uint32_t)s + 3) & ~3u);
			const int32_t ntri = S16(s, 0);
			const int32_t nquad = S16(s, 2);                                  // [ebp-0x1c]
			s += 0xC;
			for (int pass = 0; pass < 2; pass++)
			{
				int32_t k = pass == 0 ? ntri : nquad;
				if (k <= 0) continue;
				if (pass == 1) s += 4;
				const int ofs = pass == 0 ? 0 : -4;
				for (; k > 0; k--)
				{
					const uint8_t *a = vbase + 8 * (U16(s, ofs) & 0xFFF);
					const uint8_t *b = vbase + 8 * (U16(s, ofs + 2) & 0xFFF);
					const uint8_t *c = vbase + 8 * (U16(s, ofs + 4) & 0xFFF);
					uint8_t *w = WS();
					S32(w, 0xE0) = S16(b, 0) - S16(a, 0);
					S32(WS(), 0xE4) = S16(b, 2) - S16(a, 2);
					S32(WS(), 0xE8) = S16(b, 4) - S16(a, 4);
					S32(WS(), 0xF0) = S16(c, 0) - S16(a, 0);
					S32(WS(), 0xF4) = S16(c, 2) - S16(a, 2);
					S32(WS(), 0xF8) = S16(c, 4) - S16(a, 4);
					w = WS();
					xl_Cross_56BBF0(w + 0xE0, w + 0xF0, w + 0xF0);
					xl_Normalize_56BD20(WS() + 0xF0, out);
					out += 8;
					s += pass == 0 ? 0x10 : 0x14;
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB4E770 DrawModelPolygonsLit(normals) (bone+0xE1): as 0xB4DFA0 without the culling /
	// screen tests, NCLIP run (result unused), colour = 0x56CA30(normal, blk+0x5C) per polygon;
	// the normal pointer advances for every polygon. Returns it.
	// ------------------------------------------------------------------------------------
	static uint8_t *DrawModelPolygonsLit_B4E770(uint8_t *nrm)
	{
		uint8_t *d = PTR(WS(), 0x80) + 0x40;                                  // [ebp-8]
		uint8_t *stp = PTR(WS(), 0x88);                                       // [ebp-4]
		uint8_t *pk = PTR(CTX(), 0x7C);                                       // edi
		uint8_t *vb = PTR(d, 4);                                              // esi
		uint8_t *q = PTR(d, 0);                                               // ebx
		for (int32_t n = U16(d, 8); n > 0; n--, q += 0x10, nrm += 8, stp++)
		{
			if (U8(stp, 0) != 0) continue;
			uint8_t *v = vb + 8 * (U16(q, 0) & 0xFFF);
			U32(pk, 8) = U32(v, 0);
			KEY78() = U16(v, 4);
			int32_t sz = U16(v, 4);
			v = vb + 8 * (U16(q, 2) & 0xFFF);
			U32(pk, 0x10) = U32(v, 0);
			KEY7C() = U16(v, 4);
			sz += U16(v, 4);
			v = vb + 8 * (U16(q, 4) & 0xFFF);
			U32(pk, 0x18) = U32(v, 0);
			KEY74() = 0;
			KEY70() = U16(v, 4);
			sz += U16(v, 4);
			xl_NClip_56CB30(U32(pk, 8), U32(pk, 0x10), U32(pk, 0x18));
			U8(pk, 3) = 7;
			xl_NormalColor_56CA30(nrm, d + 0x1C, WS() + 0xFC);
			U32(pk, 4) = (U32(WS(), 0xFC) & 0xFFFFFF) | 0x24000000;
			U16(pk, 0xC) = U16(q, 8);
			U16(pk, 0x14) = U16(q, 0xC);
			U16(pk, 0x1C) = U16(q, 6);
			U16(pk, 0x16) = U16(q, 0xE);
			U16(pk, 0xE) = U16(q, 0xA);
			InsertPrim_B4DDD0(div3(sz) >> 4, pk);
			pk += 0x20;
		}
		for (int32_t n = U16(d, 0xA); n > 0; n--, q += 0x14, nrm += 8, stp++)
		{
			if (U8(stp, 0) != 0) continue;
			uint8_t *v = vb + 8 * (U16(q, 0) & 0xFFF);
			U32(pk, 8) = U32(v, 0);
			KEY78() = U16(v, 4);
			int32_t sz = U16(v, 4);
			v = vb + 8 * (U16(q, 2) & 0xFFF);
			U32(pk, 0x10) = U32(v, 0);
			KEY7C() = U16(v, 4);
			sz += U16(v, 4);
			v = vb + 8 * (U16(q, 4) & 0xFFF);
			U32(pk, 0x18) = U32(v, 0);
			KEY70() = U16(v, 4);
			sz += U16(v, 4);
			v = vb + 8 * (U16(q, 6) & 0xFFF);
			U32(pk, 0x20) = U32(v, 0);
			KEY74() = U16(v, 4);
			sz += U16(v, 4);
			xl_NClip_56CB30(U32(pk, 8), U32(pk, 0x10), U32(pk, 0x18));
			U8(pk, 3) = 9;
			xl_NormalColor_56CA30(nrm, d + 0x1C, WS() + 0xFC);
			U32(pk, 4) = (U32(WS(), 0xFC) & 0xFFFFFF) | 0x2C000000;
			U16(pk, 0xC) = U16(q, 8);
			U16(pk, 0x14) = U16(q, 0xC);
			U16(pk, 0x1C) = U16(q, 0x10);
			U16(pk, 0x24) = U16(q, 0x12);
			U16(pk, 0x16) = U16(q, 0xE);
			U16(pk, 0xE) = U16(q, 0xA);
			InsertPrim_B4DDD0((sz / 4) >> 4, pk);
			pk += 0x28;
		}
		PTR(CTX(), 0x7C) = pk;
		PTR(WS(), 0x88) = stp;
		return nrm;
	}

	// ------------------------------------------------------------------------------------
	// 0xB4D990 StepPieces: every live piece by mode (desc+8): 0 / 5-7 Y only (velocity +=
	// acceleration, position += velocity << 4) + spin; 1 / 3 X, Z, Y, stopped (velocities and
	// accelerations 0) once Y > 0; 2 X, Y, Z; 4 nothing. Then life - 1; a piece whose life was
	// <= 0 is freed (alive count - 1, polygon state 0xFF).
	// ------------------------------------------------------------------------------------
	static inline void StepAxis(uint8_t *p, int vel, int acc, int pos)
	{
		U16(p, vel) = (uint16_t)(U16(p, vel) + U16(p, acc));
		S32(p, pos) = add32(S32(p, pos), shl32(S16(p, vel), 4));
	}

	static void StepPieces_B4D990()
	{
		uint8_t *st = PTR(CUR(), 0xC0);                                       // esi
		uint8_t *desc = PTR(CUR(), 0xB8);                                     // edi
		int32_t n = (int32_t)U16(st, 2) + 1;
		const uint32_t mode = U16(desc, 8);
		uint8_t *p = st + 0x20;
		if (mode > 7 || mode == 4 || n <= 0) return;
		for (; n > 0; n--, p += 0x44)
		{
			if (U32(p, 0) == 0) continue;
			if (mode == 1 || mode == 3)
			{
				StepAxis(p, 0x3C, 0x3E, 4);
				StepAxis(p, 0x40, 0x42, 0xC);
				StepAxis(p, 0x16, 0x1E, 8);
				if (S32(p, 8) > 0)
				{
					U16(p, 0x1E) = 0;
					U16(p, 0x16) = 0;
					U16(p, 0x3E) = 0;
					U16(p, 0x3C) = 0;
					U16(p, 0x42) = 0;
					U16(p, 0x40) = 0;
				}
			}
			else if (mode == 2)
			{
				StepAxis(p, 0x3C, 0x3E, 4);
				StepAxis(p, 0x16, 0x1E, 8);
				StepAxis(p, 0x40, 0x42, 0xC);
			}
			else
			{
				StepAxis(p, 0x16, 0x1E, 8);
				U16(p, 0x32) = (uint16_t)(U16(p, 0x32) + U16(p, 0x34));
			}
			const int8_t life = S8(p, 0x2E);
			U8(p, 0x2E) = (uint8_t)(life - 1);
			if (life <= 0)
			{
				U32(p, 0) = 0;
				U16(st, 6) = (uint16_t)(U16(st, 6) - 1);
				U8(PTR(WS(), 0x88), U16(p, 0x30)) = 0xFF;
			}
		}
	}

	// 0xB4DDD0 InsertPiecePrim(z, prim): z < 0xFFF -> SSIGPU_InsertPrimDepthKeys(OT + 4z, prim,
	// the four depth keys 0x2796EF0 / 7C / 70 / 74)
	static void InsertPrim_B4DDD0(int32_t z, void *prim)
	{
		if (z >= 0xFFF) return;
		x::InsertPrimDepthKeys(U32(RT(), 0x4C) + 4 * (uint32_t)z, prim, KEY78(), KEY7C(), KEY70(), KEY74());
	}

	// ------------------------------------------------------------------------------------
	// 0xB4DFA0 DrawModelPolygons: the object's G3 (blk+0x48) / G4 (blk+0x4A) records (blk+0x40)
	// whose state byte (ws+0x88, advanced) is 0, at the projected vertices (blk+0x44, 8 bytes:
	// SXY, u16 depth, u16 flags), flat colour blk+0x5C (0x24 / 0x2C textured), skipped when a
	// vertex has flag 0x8000 or (G3) the polygon spans above Y 0 and below Y 0xD2.
	// ------------------------------------------------------------------------------------
	static void DrawModelPolygons_B4DFA0()
	{
		uint8_t *d = PTR(WS(), 0x80) + 0x40;                                  // [ebp-0xc]
		uint8_t *stp = PTR(WS(), 0x88);                                       // [ebp-0x10] / edx
		uint8_t *pk = PTR(CTX(), 0x7C);                                       // edi
		uint8_t *vb = PTR(d, 4);                                              // esi
		uint8_t *q = PTR(d, 0);                                               // ebx
		for (int32_t n = U16(d, 8); n > 0; n--, q += 0x10, stp++)
		{
			if (U8(stp, 0) != 0) continue;
			uint8_t *v = vb + 8 * (U16(q, 0) & 0xFFF);
			U32(pk, 8) = U32(v, 0);
			uint32_t fl = U16(v, 6) & 0x8000;
			KEY78() = U16(v, 4);
			int32_t sz = U16(v, 4);
			v = vb + 8 * (U16(q, 2) & 0xFFF);
			U32(pk, 0x10) = U32(v, 0);
			fl |= U16(v, 6) & 0x8000;
			KEY7C() = U16(v, 4);
			sz += U16(v, 4);
			v = vb + 8 * (U16(q, 4) & 0xFFF);
			U32(pk, 0x18) = U32(v, 0);
			fl |= U16(v, 6) & 0x8000;
			KEY74() = 0;
			KEY70() = U16(v, 4);
			sz += U16(v, 4);
			const int32_t y0 = S16(pk, 0xA);
			int32_t lo = y0, hi = y0;
			if (S16(pk, 0x12) < lo) lo = S16(pk, 0x12);
			if (S16(pk, 0x1A) < lo) lo = S16(pk, 0x1A);
			if (S16(pk, 0x12) > hi) hi = S16(pk, 0x12);
			if (S16(pk, 0x1A) > hi) hi = S16(pk, 0x1A);
			if (fl != 0) continue;
			if (lo < 0 && hi > 0xD2) continue;
			U8(pk, 3) = 7;
			U32(pk, 4) = (U32(d, 0x1C) & 0xFFFFFF) | 0x24000000;
			U16(pk, 0xC) = U16(q, 8);
			U16(pk, 0x14) = U16(q, 0xC);
			U16(pk, 0x1C) = U16(q, 6);
			U16(pk, 0x16) = U16(q, 0xE);
			U16(pk, 0xE) = U16(q, 0xA);
			InsertPrim_B4DDD0(div3(sz) >> 4, pk);
			pk += 0x20;
		}
		for (int32_t n = U16(d, 0xA); n > 0; n--, q += 0x14, stp++)
		{
			if (U8(stp, 0) != 0) continue;
			uint8_t *v = vb + 8 * (U16(q, 0) & 0xFFF);
			U32(pk, 8) = U32(v, 0);
			KEY78() = U16(v, 4);
			uint32_t fl = U16(v, 6) & 0x8000;
			int32_t sz = U16(v, 4);
			v = vb + 8 * (U16(q, 2) & 0xFFF);
			U32(pk, 0x10) = U32(v, 0);
			fl |= U16(v, 6) & 0x8000;
			KEY7C() = U16(v, 4);
			sz += U16(v, 4);
			v = vb + 8 * (U16(q, 4) & 0xFFF);
			U32(pk, 0x18) = U32(v, 0);
			fl |= U16(v, 6) & 0x8000;
			KEY70() = U16(v, 4);
			sz += U16(v, 4);
			v = vb + 8 * (U16(q, 6) & 0xFFF);
			U32(pk, 0x20) = U32(v, 0);
			fl |= U16(v, 6) & 0x8000;
			KEY74() = U16(v, 4);
			sz += U16(v, 4);
			if (fl != 0) continue;
			U8(pk, 3) = 9;
			U32(pk, 4) = (U32(d, 0x1C) & 0xFFFFFF) | 0x2C000000;
			U16(pk, 0xC) = U16(q, 8);
			U16(pk, 0x14) = U16(q, 0xC);
			U16(pk, 0x1C) = U16(q, 0x10);
			U16(pk, 0x24) = U16(q, 0x12);
			U16(pk, 0x16) = U16(q, 0xE);
			U16(pk, 0xE) = U16(q, 0xA);
			InsertPrim_B4DDD0((sz / 4) >> 4, pk);
			pk += 0x28;
		}
		PTR(CTX(), 0x7C) = pk;
		PTR(WS(), 0x88) = stp;
	}

	// ------------------------------------------------------------------------------------
	// 0xB4E300 DrawPieces: every live piece at its centroid + vertex offsets (no rotation),
	// projected (0x56C880) through the parent transform (0xB4BFA0 = h_B27130), colour =
	// ws+0x48 table[life] (0x26 / 0x2E: textured semi-transparent, tpage |= 0x20).
	// ------------------------------------------------------------------------------------
	static void DrawPiece(uint8_t *&pk, const uint8_t *p, int32_t px, int32_t py, int32_t pz, int8_t life)
	{
		uint8_t *prim = PTR(p, 0);                                            // [ebp-0x24]
		const int nv = U8(p, 0x26) ? 4 : 3;
		uint8_t *v = WS() + 0xF0;                                             // ebx
		const int32_t x = px >> 8, y = py >> 8, z = pz >> 8;
		int32_t sum = 0;
		for (int k = 0; k < nv; k++)
		{
			U16(v, 0) = (uint16_t)(U16(p, 0x10 + 8 * k) + (uint16_t)x);
			U16(v, 2) = (uint16_t)(U16(p, 0x12 + 8 * k) + (uint16_t)y);
			U16(v, 4) = (uint16_t)(U16(p, 0x14 + 8 * k) + (uint16_t)z);
			uint8_t *w = WS() + 0xFC;
			sum += xl_RotTransPers(v, pk + 8 + 8 * k, w, w);
			const int32_t key = shl32(S32(WS(), 0xFC), 2);
			if (k == 0) KEY78() = key;
			else if (k == 1) KEY7C() = key;
			else if (k == 2)
			{
				if (nv == 3) KEY74() = 0;
				KEY70() = key;
			}
			else KEY74() = key;
		}
		if (nv == 4)
		{
			U8(pk, 3) = 9;
			U32(pk, 4) = U32(PTR(WS(), 0x48), 4 * (int32_t)life) | 0x2E000000;
			U16(pk, 0xC) = U16(prim, 8);
			U16(pk, 0x14) = U16(prim, 0xC);
			U16(pk, 0x1C) = U16(prim, 0x10);
			U16(pk, 0x24) = U16(prim, 0x12);
			U16(pk, 0x16) = (uint16_t)(U16(prim, 0xE) | 0x20);
			U16(pk, 0xE) = U16(prim, 0xA);
			InsertPrim_B4DDD0((sum / 4) >> 2, pk);
			pk += 0x28;
		}
		else
		{
			U8(pk, 3) = 7;
			U32(pk, 4) = U32(PTR(WS(), 0x48), 4 * (int32_t)life) | 0x26000000;
			U16(pk, 0xC) = U16(prim, 8);
			U16(pk, 0x14) = U16(prim, 0xC);
			U16(pk, 0x1C) = U16(prim, 6);
			U16(pk, 0x16) = (uint16_t)(U16(prim, 0xE) | 0x20);
			U16(pk, 0xE) = U16(prim, 0xA);
			InsertPrim_B4DDD0(div3(sum) >> 2, pk);
			pk += 0x20;
		}
	}

	static void DrawPieces_B4E300()
	{
		// 30 fps layer: see mag018_elvoret_death_held.inc
		FX_HELD(if (held_active()) { DrawPiecesHeld(); return; })
		uint8_t *st = PTR(WS(), 0x84);                                        // ebx
		uint8_t *pk = PTR(CTX(), 0x7C);                                       // edi
		uint8_t *p = st + 0x20;                                               // esi
		h_B27130();
		for (int32_t n = (int32_t)U16(st, 2) + 1; n > 0; n--, p += 0x44)
			if (PTR(p, 0)) DrawPiece(pk, p, S32(p, 4), S32(p, 8), S32(p, 0xC), S8(p, 0x2E));
		PTR(CTX(), 0x7C) = pk;
	}

	// ------------------------------------------------------------------------------------
	// 0xB4DE20 DrawModel: GTE back colour = blk+0x5C (first overridden by the global colour
	// 0xB8BA00 when its byte 3 is not 0xFF); for every object whose bit is set in blk+0x60: its
	// vertex groups projected (0x50FD40, skeleton matrices) to blk+0x44, the object's G3 / G4
	// counts (+0x48..+0x4E) and records (+0x40), then its polygons still on the model (0xB4DFA0;
	// bone+0xE1: 0xB4E770 instead) and all the pieces (0xB4E300).
	// ------------------------------------------------------------------------------------
	static void DrawModel_B4DE20()
	{
		uint8_t *st = PTR(WS(), 0x84);                                        // ecx
		uint8_t *blk = PTR(WS(), 0x80);                                       // esi
		uint8_t *extra = PTR(st, 0x14);                                       // [ebp-8]
		S32(WS(), 0x60) = 0;
		uint8_t *bones = PTR(blk, 0x30) + 0x10;                               // [ebp-0x14]
		uint8_t *tab = PTR(blk, 0x34) + 4;                                    // edi
		const int32_t nobj = S32(tab, -4);                                    // [ebp-0x10]
		if (MEM<uint8_t>(0xB8BA03) != 0xFF)
		{
			U32(blk, 0x5C) = MEM<uint32_t>(0xB8BA00);
			U8(blk, 0x5F) = 0;
		}
		x::GteSetBackColor3(U8(blk, 0x5C), U8(blk, 0x5D), U8(blk, 0x5E));
		for (int32_t i = 0; i < nobj; i++)
		{
			uint8_t *o = PTR(blk, 0x34) + U32(tab, 0);                        // [ebp-4]
			tab += 4;
			if (!(U32(blk, 0x60) & (1u << i))) continue;
			uint8_t *out = PTR(blk, 0x44);                                    // [ebp-0x18]
			int32_t ng = S16(o, 0);
			o += 2;
			for (; ng > 0; ng--)
			{
				uint8_t *m = bones + S16(o, 0) * 48 + 0x10;
				o += 2;
				x::SetRotMatrix(m);
				x::SetTransVector(m);
				xl_ProjectVertexGroup_50FD40(&o, &out);
			}
			o = (uint8_t *)(((uint32_t)o + 3) & ~3u);
			U16(blk, 0x48) = U16(o, 0);
			U16(blk, 0x4A) = U16(o, 2);
			U16(blk, 0x4C) = U16(o, 4);
			U16(blk, 0x4E) = U16(o, 6);
			PTR(blk, 0x40) = o + 0xC;
			if (U8(CUR(), 0xE1)) extra = DrawModelPolygonsLit_B4E770(extra);
			else DrawModelPolygons_B4DFA0();
			DrawPieces_B4E300();
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB4DB80 Dots: every live dot (bone+0xA0) is stepped on every draw (frozen or not): life -
	// 1 (< 0: dead), then by mode: 0 / 2 / 5-7 X, Y, Z; 1 / 3 X, Z, Y with the Y velocity /
	// acceleration cleared once Y > 0; 4 no motion. Drawn as a 1-pixel dot (0x6A, colour =
	// ws+0x4C table[life]) at its position (mode 6: radius / angle -> X = sin * r, Z = cos * r)
	// into the sprite packet area (ctx+0xD8), depth keys = OTZ * 4, bucket OTZ / 4.
	// ------------------------------------------------------------------------------------
	static void DrawDot(uint8_t *pk, const uint8_t *desc, int8_t life, int32_t dx, int32_t dy, int32_t dz)
	{
		U8(pk, 3) = 2;
		U32(pk, 4) = U32(PTR(WS(), 0x4C), 4 * (int32_t)life) | 0x6A000000;
		uint8_t *v = WS() + 0xF0;                                             // edi
		if (U16(desc, 8) == 6)
		{
			const int32_t a = dz >> 4;                                        // [ebp-0xc]
			const int32_t r = dx >> 8;                                        // [ebp-4]
			U16(v, 0) = (uint16_t)(mul32(x::Sin(a), r) >> 12);
			U16(v, 4) = (uint16_t)(mul32(x::Cos(a), r) >> 12);
			U16(v, 2) = (uint16_t)(dy >> 8);
		}
		else
		{
			U16(v, 0) = (uint16_t)(dx >> 8);
			U16(v, 2) = (uint16_t)(dy >> 8);
			U16(v, 4) = (uint16_t)(dz >> 8);
		}
		uint8_t *w = WS() + 0xFC;
		const int32_t z = xl_RotTransPers(v, pk + 8, w, w);
		const int32_t key = shl32(z, 2);
		KEY78() = key;
		KEY7C() = key;
		KEY70() = key;
		KEY74() = key;
		InsertPrim_B4DDD0(z >> 2, pk);
	}

	static void Dots_B4DB80()
	{
		// 30 fps layer: see mag018_elvoret_death_held.inc
		FX_HELD(if (held_active()) { DotsHeld(); return; })
		uint8_t *d = PTR(WS(), 0x8C);                                         // esi
		uint8_t *desc = PTR(WS(), 0x90);                                      // ecx / [ebp-0x10]
		uint8_t *pk = PTR(CTX(), 0xD8);                                       // ebx
		for (int32_t n = U16(desc, 0x2E); n > 0; n--, d += 0x1C)
		{
			if (!U8(d, 0x18)) continue;
			const uint32_t mode = U16(desc, 8);
			if (mode <= 7 && mode != 4)
			{
				const int8_t life = (int8_t)(S8(d, 0x19) - 1);
				U8(d, 0x19) = (uint8_t)life;
				if (life < 0)
				{
					U8(d, 0x18) = 0;
					continue;
				}
				if (mode == 1 || mode == 3)
				{
					StepAxis(d, 0xC, 0xE, 0);
					StepAxis(d, 0x14, 0x16, 8);
					StepAxis(d, 0x10, 0x12, 4);
					if (S32(d, 4) > 0)
					{
						U16(d, 0x12) = 0;
						U16(d, 0x10) = 0;
					}
				}
				else
				{
					StepAxis(d, 0xC, 0xE, 0);
					StepAxis(d, 0x10, 0x12, 4);
					StepAxis(d, 0x14, 0x16, 8);
				}
			}
			DrawDot(pk, desc, S8(d, 0x19), S32(d, 0), S32(d, 4), S32(d, 8));
			pk += 0xC;
		}
		PTR(CTX(), 0xD8) = pk;
	}

	// ------------------------------------------------------------------------------------
	// 0xB4BBE0 (Draw 22 EntityDisintegrate), see the module comment. First call: model copy of
	// the slot entity (bone+0xBC), piece block (bone+0xC0), dots (bone+0xA0, when desc+0x2E),
	// polygon state bytes (bone+0xC4), mode 5: per-polygon values (0xB4D5F0), bone+0xE1: per-
	// polygon extra block (0xB4C130), entity flag +0 |= 4. Every call: ws+0x80..0x90 = the
	// blocks, ws+0x48 / 0x4C = the colour tables; copy re-synced, entity bone matrices from its
	// pose; not frozen: ws+0x40 / 0x44 = the counter period (polygons still on the model /
	// (outPos Z + 1), at least 1; 0x7FFF when outPos Z is 0), ws+0x50.. mode / outPos X / Z,
	// ws+0xF0 = outPos Y, update (0xB4C530) and step (0xB4D990); then the parent transform
	// (0xB4BFA0), the skeleton's world matrices, the model + pieces (0xB4DE20), the dots
	// (0xB4DB80) and the copy's frame counter + 1.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_22_EntityDisintegrate()
	{
		// 30 fps layer: see mag018_elvoret_death_held.inc
		FX_HELD(if (held_active()) { dh_22_EntityDisintegrateHeld(); return; } NoteLightN();)
		if (U32(CUR(), 0xBC) == 0)
		{
			uint8_t *desc = PTR(CUR(), 0xB8);                                 // ebx
			PTR(CUR(), 0xBC) = CreateEntityModelCopy_B4C480(SlotEntity());
			const int32_t slots = U16(desc, 0xA);                             // edi
			PTR(CUR(), 0xC0) = blob::ArenaAlloc(slots * 0x44 + 0x20);
			uint8_t *st = PTR(CUR(), 0xC0);                                   // esi
			U16(st, 0) = 0;
			U16(st, 2) = (uint16_t)(slots - 1);
			U16(st, 6) = 0;
			PTR(st, 8) = desc + S16(desc, 0);
			PTR(st, 0xC) = desc + S16(desc, 2);
			PTR(st, 0x10) = desc + S16(desc, 4);
			if (U16(desc, 0x2E) != 0)
			{
				U16(st, 0x1C) = 0;
				const int32_t nd = U16(desc, 0x2E);
				PTR(CUR(), 0xA0) = blob::ArenaAlloc(nd * 0x1C);
				uint8_t *d = PTR(CUR(), 0xA0);
				for (int32_t k = nd; k > 0; k--, d += 0x1C) U8(d, 0x18) = 0;
			}
			uint8_t *blk = PTR(CUR(), 0xBC);                                  // [ebp-4]
			CountModelPolygons_B4C090(blk + 0x30);
			const int32_t npoly = add32(S32(WS(), 0xFC), S32(WS(), 0xF8));
			U16(st, 4) = (uint16_t)npoly;
			const int32_t nb = add32(npoly, 4) & ~3;                          // edi
			PTR(CUR(), 0xC4) = blob::ArenaAlloc(nb);
			uint8_t *sb = PTR(CUR(), 0xC4);
			if (nb > 0) memset(sb, 0, 4 * (size_t)((uint32_t)(nb + 3) >> 2));
			uint8_t *p = st + 0x20;
			for (int32_t k = (int32_t)U16(st, 2) + 1; k > 0; k--, p += 0x44) U32(p, 0) = 0;
			if (U16(desc, 8) == 5)
			{
				uint8_t *b = blob::ArenaAlloc(4 * (int32_t)U16(st, 4));
				PTR(st, 0x18) = b;
				PolygonValues_B4D5F0(blk + 0x10, b);
			}
			if (U8(CUR(), 0xE1))
			{
				uint8_t *b = blob::ArenaAlloc(8 * (int32_t)U16(st, 4));
				PTR(st, 0x14) = b;
				PolygonNormals_B4C130(blk + 0x10, b);
			}
			else
				PTR(st, 0x14) = nullptr;
			U8(SlotEntity(), 0) |= 4;
		}
		uint8_t *bone = CUR();
		PTR(WS(), 0x90) = PTR(bone, 0xB8);
		PTR(WS(), 0x80) = PTR(CUR(), 0xBC);
		PTR(WS(), 0x84) = PTR(CUR(), 0xC0);
		uint8_t *st = PTR(WS(), 0x84);
		PTR(WS(), 0x88) = PTR(CUR(), 0xC4);
		PTR(WS(), 0x8C) = PTR(CUR(), 0xA0);
		U32(WS(), 0x48) = U32(st, 8);
		U32(WS(), 0x4C) = U32(st, 0xC);
		SyncEntityModelCopy_B4C4E0(PTR(CUR(), 0xBC));
		x::BuildBoneMatricesFromPose(PTR(WS(), 0x80) + 0x10);
		if (U8(RT(), 0x45) == 0)
		{
			uint8_t *w = WS();
			st = PTR(w, 0x84);                                                // esi
			S32(w, 0x50) = U16(PTR(w, 0x90), 8);
			S32(WS(), 0x54) = S16(CUR(), 0x94);
			S32(WS(), 0x58) = S16(CUR(), 0x98);
			S32(WS(), 0x5C) = 0;
			w = WS();                                                         // ebx
			const int32_t zz = S16(CUR(), 0x98);                              // edi
			int32_t per;
			if (zz != 0)
			{
				const uint8_t *sb = PTR(w, 0x88);
				int32_t left = 0;
				for (int32_t k = U16(st, 4); k > 0; k--, sb++)
					if (U8(sb, 0) == 0) left++;
				per = left / (zz + 1);
				if (per == 0) per = 1;
			}
			else
				per = 0x7FFF;
			S32(w, 0x44) = per;
			S32(WS(), 0x40) = per;
			S32(WS(), 0xF0) = S16(CUR(), 0x96);
			UpdatePolygons_B4C530();
			StepPieces_B4D990();
		}
		uint8_t *blk = PTR(WS(), 0x80);                                       // esi
		h_B27130();
		x::ComputeBonesWorldMatrices(blk + 0x10, WS() + 0xE0);
		U32(blk, 0x44) = U32(CTX(), 0x74);
		DrawModel_B4DE20();
		Dots_B4DB80();
		U16(blk, 0) = (uint16_t)(U16(blk, 0) + 1);
	}

	// ------------------------------------------------------------------------------------
	// handler set: the shared handlers this module carries (apply_shared_misc: only draw 16 /
	// prim 16 / VM 0x042 are taken), then this module's own handlers
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn d16 = c.draw[16];        // 0xB4B450 = Leviathan 0xB58FA0 / Bahamut 0xB198C0 ScrollTextureU
		const VoidFn p16 = c.prim[16];        // 0xB50540 = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
		const VoidFn v042 = c.vm[0x042];      // 0xB57280 = Bahamut 0xB24BB0 EntityChainAnim
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[16] = d16;
		c.prim[16] = p16;
		c.vm[0x042] = v042;
		// VM 0x0AF 0xB52F20 SetInlineDescriptor: not reached by this script: kept original (init_clone:
		// Ifrit's slot is a stub, so no generic port), as Final Form Death keeps its 0xB39820
		c.draw[22] = dh_22_EntityDisintegrate;          // 0xB4BBE0 (missing function start in the IDB)
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag018_elvoret_death()
	{
		using namespace gfc;
		elvoret018::describe(elvoret018::g_c);
		init_clone(elvoret018::g_c, nullptr, 0);
		elvoret018::apply_slots(elvoret018::g_c);
		register_port(0xB4ABA0, (void *)elvoret018::SequenceTask, "MAG018 Elvoret Death SequenceTick", 18);
		// 30 fps layer: see mag018_elvoret_death_held.inc
		FX_HELD(register_mag018_elvoret_death_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag018_elvoret_death_held.inc"
#endif
