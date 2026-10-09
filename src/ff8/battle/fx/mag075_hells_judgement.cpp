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

// Effect 75: Hell's Judgement (enemy attack 358 of kernel.bin, Ultimecia c0m126; no other
// kernel.bin entry uses effect 75), cinematic-engine module MAG_075 0xB3E8B0..0xB4A500, script
// data magic/mag074_b.00/.01 (loaded by MAG_075_HELLS_JUDGEMENT_FL 0xB3E8B0).
//
// Hell's Judgement runs the GF cinematic engine (gfc_engine.cpp): the module is one more compile
// of the engine library. Instruction-level comparison against the ported clones (module-relative
// addresses normalised, jump tables and callee trees hashed):
//   - SetupSummon 0xB3E8E0 is a byte clone of Water's / Meteor's; SequenceTick 0xB3EF50 = Water's
//     (the unlit BuildMatricesAndDraw 0xB45190), lit_dispatcher false;
//   - every table handler (VM, bone, prim, sprite, particle type, particle VM, draw) is a byte
//     clone of the generic (Ifrit) one or a `ret` stub, except
//       VM 0x006 LoadBattleFile 0xB45E80: same code, own load callback 0xB45FB0 (c.load_cb),
//       draw 61 0xB3F9C0: MeshRadialLightWave, a byte clone of Eden's 0xAE71D0 (apply_eden_b in
//         mag_eden.cpp; only that slot is taken: Hell's Judgement's draw 62 is a stub),
//       draw 66 0xB429A0: ShatterMesh, unique to this module, ported below.
// The effect's queue (0x2796E88) holds one task, the engine tick 0xB3EF50.
// The effect never writes a battle entity's position (+0x1C..+0x20); the battle camera is
// written only by the engine (VM 0x039, gfc_engine.cpp).
//
// Draw 66 ShatterMesh breaks a static mesh into flying polygons. desc = bone+0xB8 {s16 offset
// of a colour table, ?, s16 object id (+4), u8 colour count / start state (+6), s16 centre x / y
// / z (+8..+0xC), then 9 velocity bases / ranges (+0xE..+0x24) and 3 x 2 spin bases / ranges
// (+0x26..+0x30)}. First call: block = bone+0xBC (16 B arena: +0 colour table, +4 piece array,
// +8 mesh, +0xC piece count) and one 0x30-byte piece per polygon of the mesh's 8 (G3) / 0x12
// (G4) lists (ShatterInit 0xB43040: centroid, distance from the centre, state 0xFF). Every
// draw: the 18 velocity / spin parameters = desc value * outPos.y / 256; each piece still whole
// (state < 0) whose distance is below outPos.x is launched (9 x 0xB65740 random spreads, position
// scaled by outAngle / 256, state = desc+6); the mesh vertices are projected with the parent
// matrix (0xB40DF0 = the engine's SetupParentXformScaledAtOrigin); ShatterTris 0xB43360 /
// ShatterQuads 0xB43680 draw each polygon: whole -> at its projected vertices with its mesh
// colour; flying -> ShatterStep 0xB43A20 (velocity += acceleration, position += velocity << 4,
// spin angles += spin speed) then rotated about its centroid, colour = table[state], state - 1;
// state 0 -> gone.
// The draw advances the pieces on every draw (it is not gated by the engine's freeze flag).
//
// Piece (0x30 bytes): +0 / +4 / +8 position (x256), +0x0C / +0x10 / +0x14 velocity x / y / z,
// +0x0E / +0x12 / +0x16 acceleration, +0x18 / +0x1A / +0x1C spin angles (x16), +0x1E / +0x20 /
// +0x22 spin speed, +0x24 / +0x26 / +0x28 centroid, +0x2A distance from the centre, +0x2C state.

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag075_hells_judgement_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_eden_b(Clone &c);  // mag_eden.cpp

namespace hells_judgement
{
	static Clone g_hj = {};

	static void describe(Clone &c)
	{
		c.name = "075 Hell's Judgement";
		c.effect_id = 75;
		c.vm_table = 0x1876338;     // MAG_075 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1875FA8;   // MAG_075 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1875FF0;   // bone_table + 0x48
		c.prim_size = 0x18760E0;    // bone_table + 0x138
		c.spr_table = 0x18760F4;    // bone_table + 0x14C
		c.ptype_table = 0x1876120;  // bone_table + 0x178
		c.pop_table = 0x1876134;    // bone_table + 0x18C
		c.draw_table = 0x1876194;   // MAG_075 DrawHandlerTable
		c.attr_16A = 0x18762FE;
		c.attr_184 = 0x1876318;
		c.attr_186 = 0x187631A;
		c.attr_194 = 0x1876328;
		c.attr_19C = 0x1876330;
		c.desc = 0x1875EAC;         // draw_table - 0x2E8 (read by 0xB3EA30)
		c.ptr_block = 0x1875F80;    // draw_table - 0x214 (read by 0xB3E990)
		c.queue = 0x2796E88;
		c.file00 = 0x2796EBC;
		c.file01 = 0x2796EB8;
		c.tick = 0xB3EF50;
		c.load_cb = 0xB45FB0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// engine calls of draw 66 not wrapped by gfc_engine.h
	// ------------------------------------------------------------------------------------
	// 0x56C910 GTE_RotTransPers3(v0, v1, v2, sxy0, sxy1, sxy2, p, flag): returns OTZ
	static inline int32_t xl_RotTransPers3(const void *v0, const void *v1, const void *v2, void *s0, void *s1, void *s2, void *p, void *flag)
	{
		return x::f<int32_t (__cdecl *)(const void *, const void *, const void *, void *, void *, void *, void *, void *)>(0x56C910)(v0, v1, v2, s0, s1, s2, p, flag);
	}
	// 0x56C970 RotTransPers4(v0, v1, v2, v3, sxy0, sxy1, sxy2, sxy3, p, flag): returns OTZ
	static inline int32_t xl_RotTransPers4(const void *v0, const void *v1, const void *v2, const void *v3, void *s0, void *s1, void *s2, void *s3, void *p, void *flag)
	{
		return x::f<int32_t (__cdecl *)(const void *, const void *, const void *, const void *, void *, void *, void *, void *, void *, void *)>(0x56C970)(v0, v1, v2, v3, s0, s1, s2, s3, p, flag);
	}
	// 0x56BEC0 (int)sqrt((double)v)
	static inline int32_t xl_Sqrt(int32_t v) { return x::f<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	// 0x45C7A0 SSIGPU_InsertPrimAutoDepth(ot, prim)
	static inline void xl_InsertPrim(uint32_t ot, void *prim) { x::f<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(ot, prim); }

	// x / 3 as compiled (imul 0x55555556, + sign bit of the high half)
	static inline int32_t div3(int32_t v)
	{
		int32_t hi = (int32_t)(((int64_t)v * 0x55555556LL) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// ------------------------------------------------------------------------------------
	// 0xB43040 ShatterInit(pieces, mesh, desc): one piece per polygon of the mesh's G3 (8) / G4
	// (0x12) lists: centroid (+0x24..+0x28, and x256 at +0 / +4 / +8), distance from the centre
	// desc+8 / +0xA / +0xC (+0x2A); G4 pieces also get spin angles 0. Returns the piece count.
	// (The end test `type == 0xFFFF` compares the sign-extended word and never matches: any type
	// other than 8 / 0x12 ends the lists.)
	// ------------------------------------------------------------------------------------
	static int32_t ShatterInit(uint8_t *p, uint8_t *mesh, uint8_t *desc)
	{
		uint8_t *ws = WS();                                           // ebx
		int32_t count = 0;                                            // eax
		uint8_t *lst = mesh + U32(mesh, 8);                           // edx
		uint8_t *verts = mesh + U32(mesh, 0x14);                      // ecx, [ebp-4]
		S32(ws, 0xF0) = S16(desc, 8);
		S32(WS(), 0xF4) = S16(desc, 0xA);
		S32(WS(), 0xF8) = S16(desc, 0xC);
		int32_t type = S16(lst, 0);                                   // ebx
		int32_t n = S16(lst, 2);                                      // edi
		if (type == 0xFFFF) return count;
		for (;;)
		{
			if (type == 8)
			{
				lst += 4;
				count = add32(count, n);
				for (int32_t k = n; k > 0; k--)
				{
					const uint8_t *a = verts + U16(lst, 0xA);
					int32_t sx = S16(a, 0), sy = S16(a, 2), sz = S16(a, 4);
					const uint8_t *b = verts + U16(lst, 0xC);
					sx += S16(b, 0);
					sy += S16(b, 2);
					sz += S16(b, 4);
					const uint8_t *c = verts + U16(lst, 0xE);
					int32_t cx = div3(S16(c, 0) + sx);
					int32_t cy = div3(S16(c, 2) + sy);
					int32_t cz = div3(S16(c, 4) + sz);
					U16(p, 0x24) = (uint16_t)cx;
					S32(p, 0) = shl32(cx, 8);
					U16(p, 0x26) = (uint16_t)cy;
					U16(p, 0x28) = (uint16_t)cz;
					S32(p, 4) = shl32(cy, 8);
					S32(p, 8) = shl32(cz, 8);
					uint8_t *w = WS();
					int32_t dx = sub32(cx, S32(w, 0xF0));
					int32_t dy = sub32(cy, S32(w, 0xF4));
					int32_t dz = sub32(cz, S32(w, 0xF8));
					int32_t r = xl_Sqrt(add32(mul32(dy, dy), mul32(dx, dx)));
					r = xl_Sqrt(add32(mul32(dz, dz), mul32(r, r)));
					U16(p, 0x2A) = (uint16_t)r;
					lst += 0x14;
					p += 0x30;
				}
			}
			else if (type == 0x12)
			{
				lst += 4;
				count = add32(count, n);
				for (int32_t k = n; k > 0; k--)
				{
					const uint8_t *a = verts + U16(lst, 0xC);
					int32_t sx = S16(a, 0), sy = S16(a, 2), sz = S16(a, 4);
					const uint8_t *b = verts + U16(lst, 0xE);
					sx += S16(b, 0);
					sy += S16(b, 2);
					sz += S16(b, 4);
					const uint8_t *c = verts + U16(lst, 0x10);
					sx += S16(c, 0);
					sy += S16(c, 2);
					sz += S16(c, 4);
					const uint8_t *d = verts + U16(lst, 0x12);
					int32_t cx = (S16(d, 0) + sx) / 4;
					int32_t cy = (S16(d, 2) + sy) / 4;
					int32_t cz = (S16(d, 4) + sz) / 4;
					U16(p, 0x24) = (uint16_t)cx;
					S32(p, 0) = shl32(cx, 8);
					U16(p, 0x26) = (uint16_t)cy;
					U16(p, 0x28) = (uint16_t)cz;
					S32(p, 4) = shl32(cy, 8);
					S32(p, 8) = shl32(cz, 8);
					uint8_t *w = WS();
					int32_t dx = sub32(cx, S32(w, 0xF0));
					int32_t dy = sub32(cy, S32(w, 0xF4));
					int32_t dz = sub32(cz, S32(w, 0xF8));
					int32_t r = xl_Sqrt(add32(mul32(dy, dy), mul32(dx, dx)));
					r = xl_Sqrt(add32(mul32(dz, dz), mul32(r, r)));
					U16(p, 0x2A) = (uint16_t)r;
					U16(p, 0x1C) = 0;
					U16(p, 0x1A) = 0;
					U16(p, 0x18) = 0;
					lst += 0x18;
					p += 0x30;
				}
			}
			else
				return count;
			type = S16(lst, 0);
			n = S16(lst, 2);
			if (type == 0xFFFF) return count;
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB43A20 ShatterStep(piece): GTE R/TR = parent (ws+0x60); velocity += acceleration,
	// position += velocity << 4; TR = parent * (position >> 8) (MVMVA -> 0x45E580); spin angles
	// += spin speed; ws+0xD0 = Rot(angles << 4) * parent.rot, scaled by outAngle << 4; GTE R =
	// ws+0xD0.
	// ------------------------------------------------------------------------------------
	// the two transform parts of ShatterStep: TR = parent * (position >> 8) ...
	static void ShatterXformPos(int32_t px, int32_t py, int32_t pz)
	{
		uint8_t *w = WS();
		S16(w, 0xC0) = (int16_t)(px >> 8);
		S16(WS(), 0xC2) = (int16_t)(py >> 8);
		S16(WS(), 0xC4) = (int16_t)(pz >> 8);
		x::GteLoadV0(WS() + 0xC0);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
	}

	// ... and GTE R = Rot(angles) * parent.rot, scaled by outAngle << 4 (ws+0xD0)
	static void ShatterXformRot(int16_t ax, int16_t ay, int16_t az)
	{
		S16(WS(), 0xC0) = ax;
		S16(WS(), 0xC2) = ay;
		S16(WS(), 0xC4) = az;
		x::RotMatrixFromAngles(WS() + 0xC0, WS() + 0xD0);
		x::MulMatrixInPlace(WS() + 0xD0, PTR(WS(), 0x60));
		pad_after_rot_mul(WS() + 0xD0, WS() + 0xC0);
		S32(WS(), 0xC0) = shl32(S16(CUR(), 0x8C), 4);
		S32(WS(), 0xC4) = shl32(S16(CUR(), 0x8E), 4);
		S32(WS(), 0xC8) = shl32(S16(CUR(), 0x90), 4);
		x::ScaleMatrix(WS() + 0xD0, WS() + 0xC0);
		x::SetRotMatrix(WS() + 0xD0);
	}

	static void ShatterStep(uint8_t *p)
	{
		x::SetRotMatrix(PTR(WS(), 0x60));
		x::SetTransVector(PTR(WS(), 0x60));
		U16(p, 0xC) = (uint16_t)(U16(p, 0xC) + U16(p, 0xE));
		U16(p, 0x10) = (uint16_t)(U16(p, 0x10) + U16(p, 0x12));
		int32_t px = add32(S32(p, 0), shl32(S16(p, 0xC), 4));
		int32_t py = add32(S32(p, 4), shl32(S16(p, 0x10), 4));
		U16(p, 0x14) = (uint16_t)(U16(p, 0x14) + U16(p, 0x16));
		S32(p, 0) = px;
		int32_t pz = add32(S32(p, 8), shl32(S16(p, 0x14), 4));
		S32(p, 4) = py;
		S32(p, 8) = pz;
		ShatterXformPos(px, py, pz);
		U16(p, 0x18) = (uint16_t)(U16(p, 0x18) + U16(p, 0x1E));
		U16(p, 0x1A) = (uint16_t)(U16(p, 0x1A) + U16(p, 0x20));
		U16(p, 0x1C) = (uint16_t)(U16(p, 0x1C) + U16(p, 0x22));
		ShatterXformRot((int16_t)(U16(p, 0x18) << 4), (int16_t)(S16(p, 0x1A) << 4), (int16_t)(S16(p, 0x1C) << 4));
	}

	// OT bucket of a piece: ((v / 3) >> 2) & ~3 (G3), ((v / 4) >> 2) & ~3 (G4), + the OT base
	static inline uint32_t BucketG3(int32_t v) { return (uint32_t)((div3(v) >> 2) & ~3) + U32(RT(), 0x4C); }
	static inline uint32_t BucketG4(int32_t v) { return (uint32_t)(((v / 4) >> 2) & ~3) + U32(RT(), 0x4C); }

	// ------------------------------------------------------------------------------------
	// 0xB43360 ShatterTris(pieces, verts, mesh): the G3 list at ws+0xF0 (count at +2, records of
	// 0x14 B from +4: +0 colour word, +4 / +6 / +8 / +0x10 / +0x12 UV / CLUT / tpage words, +0xA /
	// +0xC / +0xE vertex offsets). Packets of 0x20 B at ctx+0x7C (code 0x24 | semi-transparency of
	// bone+0xCC bit 25, tpage | bone+0x92 & 0x60). Returns the next piece, ws+0xF0 = next list.
	// ------------------------------------------------------------------------------------
	static uint8_t *ShatterTris(uint8_t *p, uint8_t *verts, uint8_t *mesh)
	{
		uint8_t *lst = PTR(WS(), 0xF0);                               // ebx
		uint8_t *pk = PTR(CTX(), 0x7C);                               // edi
		S32(WS(), 0xF0) = S16(lst, 2);
		lst += 4;
		U32(WS(), 0xF4) = (U32(CUR(), 0xCC) & 0x2000000) | 0x24000000;
		U32(WS(), 0xF8) = U8(CUR(), 0x92) & 0x60;
		PTR(WS(), 0xEC) = mesh + U32(mesh, 0x14);
		uint8_t *start = pk;                                          // [ebp-4]
		if (S32(WS(), 0xF0) > 0)
		{
			pk += 0x10;
			do
			{
				int8_t s = S8(p, 0x2C);
				if (s != 0)
				{
					int32_t z;                                        // ecx
					if (s > 0)
					{
						ShatterStep(p);
						uint8_t *w = WS();
						const uint8_t *v = PTR(w, 0xEC) + U16(lst, 0xA);
						U16(w, 0xC0) = (uint16_t)(U16(v, 0) - U16(p, 0x24));
						U16(WS(), 0xC2) = (uint16_t)(U16(v, 2) - U16(p, 0x26));
						U16(WS(), 0xC4) = (uint16_t)(U16(v, 4) - U16(p, 0x28));
						v = PTR(WS(), 0xEC) + U16(lst, 0xC);
						U16(WS(), 0xC8) = (uint16_t)(U16(v, 0) - U16(p, 0x24));
						U16(WS(), 0xCA) = (uint16_t)(U16(v, 2) - U16(p, 0x26));
						U16(WS(), 0xCC) = (uint16_t)(U16(v, 4) - U16(p, 0x28));
						v = PTR(WS(), 0xEC) + U16(lst, 0xE);
						U16(WS(), 0xD0) = (uint16_t)(U16(v, 0) - U16(p, 0x24));
						U16(WS(), 0xD2) = (uint16_t)(U16(v, 2) - U16(p, 0x26));
						U16(WS(), 0xD4) = (uint16_t)(U16(v, 4) - U16(p, 0x28));
						w = WS();
						int32_t otz = xl_RotTransPers3(w + 0xC0, w + 0xC8, w + 0xD0, pk - 8, pk, pk + 8, w + 0xFC, w + 0xFC);
						z = shl32(mul32(otz, 3), 2);
						U32(pk, -0xC) = U32(PTR(WS(), 0x64), (int32_t)S8(p, 0x2C) * 4) | U32(WS(), 0xF4);
						U8(p, 0x2C) = (uint8_t)(U8(p, 0x2C) - 1);
					}
					else
					{
						U32(pk, -8) = U32(verts, U16(lst, 0xA));
						U32(pk, 0) = U32(verts, U16(lst, 0xC));
						U32(pk, 8) = U32(verts, U16(lst, 0xE));
						z = (int32_t)U16(verts, U16(lst, 0xC) + 4) + U16(verts, U16(lst, 0xA) + 4);
						z += U16(verts, U16(lst, 0xE) + 4);
						U32(pk, -0xC) = U32(WS(), 0xF4) | U32(lst, 0);
					}
					U8(pk, -0xD) = 7;
					U16(pk, 6) = (uint16_t)(U16(WS(), 0xF8) | U16(lst, 0x12));
					U16(pk, -2) = U16(lst, 0x10);
					U16(pk, -4) = U16(lst, 4);
					U16(pk, 4) = U16(lst, 6);
					U16(pk, 0xC) = U16(lst, 8);
					xl_InsertPrim(BucketG3(z), start);
					start += 0x20;
					pk += 0x20;
				}
				S32(WS(), 0xF0) = S32(WS(), 0xF0) - 1;
				lst += 0x14;
				p += 0x30;
			} while (S32(WS(), 0xF0) > 0);
		}
		PTR(CTX(), 0x7C) = start;
		PTR(WS(), 0xF0) = lst;
		return p;
	}

	// ------------------------------------------------------------------------------------
	// 0xB43680 ShatterQuads(pieces, verts, mesh): the G4 list at ws+0xF0 (records of 0x18 B: +0
	// colour word, +4..+0xA / +0x14 / +0x16 UV / CLUT / tpage words, +0xC..+0x12 vertex offsets),
	// packets of 0x28 B (code 0x2C). As ShatterTris otherwise.
	// ------------------------------------------------------------------------------------
	static uint8_t *ShatterQuads(uint8_t *p, uint8_t *verts, uint8_t *mesh)
	{
		uint8_t *lst = PTR(WS(), 0xF0);                               // ebx
		uint8_t *pk = PTR(CTX(), 0x7C);                               // edi
		S32(WS(), 0xF0) = S16(lst, 2);
		lst += 4;
		U32(WS(), 0xF4) = (U32(CUR(), 0xCC) & 0x2000000) | 0x2C000000;
		U32(WS(), 0xF8) = U8(CUR(), 0x92) & 0x60;
		PTR(WS(), 0xEC) = mesh + U32(mesh, 0x14);
		uint8_t *start = pk;                                          // [ebp-4]
		if (S32(WS(), 0xF0) > 0)
		{
			pk += 0x10;
			do
			{
				int8_t s = S8(p, 0x2C);
				if (s != 0)
				{
					int32_t z;                                        // eax
					if (s > 0)
					{
						ShatterStep(p);
						uint8_t *w = WS();
						const uint8_t *v = PTR(w, 0xEC) + U16(lst, 0xC);
						U16(w, 0xC0) = (uint16_t)(U16(v, 0) - U16(p, 0x24));
						U16(WS(), 0xC2) = (uint16_t)(U16(v, 2) - U16(p, 0x26));
						U16(WS(), 0xC4) = (uint16_t)(U16(v, 4) - U16(p, 0x28));
						v = PTR(WS(), 0xEC) + U16(lst, 0xE);
						U16(WS(), 0xC8) = (uint16_t)(U16(v, 0) - U16(p, 0x24));
						U16(WS(), 0xCA) = (uint16_t)(U16(v, 2) - U16(p, 0x26));
						U16(WS(), 0xCC) = (uint16_t)(U16(v, 4) - U16(p, 0x28));
						v = PTR(WS(), 0xEC) + U16(lst, 0x10);
						U16(WS(), 0xD0) = (uint16_t)(U16(v, 0) - U16(p, 0x24));
						U16(WS(), 0xD2) = (uint16_t)(U16(v, 2) - U16(p, 0x26));
						U16(WS(), 0xD4) = (uint16_t)(U16(v, 4) - U16(p, 0x28));
						v = PTR(WS(), 0xEC) + U16(lst, 0x12);
						U16(WS(), 0xD8) = (uint16_t)(U16(v, 0) - U16(p, 0x24));
						U16(WS(), 0xDA) = (uint16_t)(U16(v, 2) - U16(p, 0x26));
						U16(WS(), 0xDC) = (uint16_t)(U16(v, 4) - U16(p, 0x28));
						w = WS();
						int32_t otz = xl_RotTransPers4(w + 0xC0, w + 0xC8, w + 0xD0, w + 0xD8, pk - 8, pk, pk + 8, pk + 0x10, w + 0xFC, w + 0xFC);
						z = shl32(otz, 4);
						U32(pk, -0xC) = U32(PTR(WS(), 0x64), (int32_t)S8(p, 0x2C) * 4) | U32(WS(), 0xF4);
						U8(p, 0x2C) = (uint8_t)(U8(p, 0x2C) - 1);
					}
					else
					{
						U32(pk, -8) = U32(verts, U16(lst, 0xC));
						U32(pk, 0) = U32(verts, U16(lst, 0xE));
						U32(pk, 8) = U32(verts, U16(lst, 0x10));
						U32(pk, 0x10) = U32(verts, U16(lst, 0x12));
						z = (int32_t)U16(verts, U16(lst, 0xE) + 4) + U16(verts, U16(lst, 0xC) + 4);
						z += U16(verts, U16(lst, 0x10) + 4);
						z += U16(verts, U16(lst, 0x12) + 4);
						U32(pk, -0xC) = U32(WS(), 0xF4) | U32(lst, 0);
					}
					U8(pk, -0xD) = 9;
					U16(pk, 6) = (uint16_t)(U16(WS(), 0xF8) | U16(lst, 0x16));
					U16(pk, -2) = U16(lst, 0x14);
					U16(pk, -4) = U16(lst, 4);
					U16(pk, 4) = U16(lst, 6);
					U16(pk, 0xC) = U16(lst, 8);
					U16(pk, 0x14) = U16(lst, 0xA);
					xl_InsertPrim(BucketG4(z), start);
					start += 0x28;
					pk += 0x28;
				}
				S32(WS(), 0xF0) = S32(WS(), 0xF0) - 1;
				lst += 0x18;
				p += 0x30;
			} while (S32(WS(), 0xF0) > 0);
		}
		PTR(CTX(), 0x7C) = start;
		PTR(WS(), 0xF0) = lst;
		return p;
	}

	// ------------------------------------------------------------------------------------
	// 0xB429A0 (Draw 66 ShatterMesh), see the top of the file.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_66_ShatterMesh()
	{
		// 30 fps layer: see mag075_hells_judgement_held.inc
		FX_HELD(if (held_active()) { dh_66_ShatterMeshHeld(); return; })
		uint8_t *bone = CUR();                                         // eax
		if (PTR(bone, 0xBC) == nullptr)
		{
			uint8_t *desc = PTR(bone, 0xB8);                           // esi
			uint8_t *blk = blob::ArenaAlloc(0x10);
			PTR(CUR(), 0xBC) = blk;
			blk = PTR(CUR(), 0xBC);                                    // edi
			uint8_t *mesh = blob::ObjectPtr(S16(desc, 4));
			PTR(blk, 8) = mesh;
			PTR(blk, 0) = desc + S16(desc, 0);
			S32(WS(), 0xF0) = S16(desc, 8);
			S32(WS(), 0xF4) = S16(desc, 0xA);
			S32(WS(), 0xF8) = S16(desc, 0xC);
			S32(WS(), 0xFC) = 0;
			uint8_t *top = PTR(CTX(), 0x74);
			PTR(blk, 4) = top;
			int32_t n = ShatterInit(top, mesh, desc);
			U16(blk, 0xC) = (uint16_t)n;
			n &= 0xFFFF;
			blob::ArenaAlloc(n * 0x30);
			uint8_t *p = PTR(blk, 4);
			for (int32_t k = n; k > 0; k--, p += 0x30) U8(p, 0x2C) = 0xFF;
			bone = CUR();
		}
		uint8_t *blk = PTR(bone, 0xBC);                                // ebx, [ebp-0xC]
		uint8_t *desc = PTR(bone, 0xB8);                               // edi
		uint8_t *mesh = PTR(blk, 8);                                   // [ebp-8]
		PTR(WS(), 0x60) = (uint8_t *)blob::GetParentMatrix(U16(bone, 0x9C));
		PTR(WS(), 0x64) = PTR(blk, 0);
		// velocity / spin parameters: desc value * outPos.y / 256
		static const uint8_t PARAM[18][2] = {
			{ 0x0E, 0xA0 }, { 0x16, 0xA2 }, { 0x1E, 0xA4 }, { 0x10, 0xA8 }, { 0x18, 0xAA }, { 0x20, 0xAC },
			{ 0x12, 0xB0 }, { 0x1A, 0xB2 }, { 0x22, 0xB4 }, { 0x14, 0xB8 }, { 0x1C, 0xBA }, { 0x24, 0xBC },
			{ 0x26, 0xC0 }, { 0x2A, 0xC2 }, { 0x2E, 0xC4 }, { 0x28, 0xC8 }, { 0x2C, 0xCA }, { 0x30, 0xCC } };
		const int32_t ky = S16(CUR(), 0x96);                           // ecx
		for (int i = 0; i < 18; i++)
			S16(WS(), PARAM[i][1]) = (int16_t)((S16(desc, PARAM[i][0]) * ky) / 256);
		uint8_t *p = PTR(blk, 4);                                      // esi
		S32(WS(), 0xF0) = S16(CUR(), 0x94);
		S32(WS(), 0xF4) = S16(CUR(), 0x8C);
		S32(WS(), 0xF8) = S16(CUR(), 0x8E);
		S32(WS(), 0xFC) = S16(CUR(), 0x90);
		int32_t n = U16(blk, 0xC);
		for (; n > 0; n--, p += 0x30)
		{
			uint8_t *w = WS();
			if (S8(p, 0x2C) >= 0 || S16(p, 0x2A) >= S32(w, 0xF0)) continue;
			// launch: 9 random spreads in this order
			int32_t r;
			r = blob::Rand73(S16(WS(), 0xA8));
			U16(p, 0xC) = (uint16_t)(U16(WS(), 0xA0) + r);
			r = blob::Rand73(S16(WS(), 0xB8));
			U16(p, 0xE) = (uint16_t)(U16(WS(), 0xB0) + r);
			r = blob::Rand73(S16(WS(), 0xAA));
			U16(p, 0x10) = (uint16_t)(U16(WS(), 0xA2) + r);
			r = blob::Rand73(S16(WS(), 0xBA));
			U16(p, 0x12) = (uint16_t)(U16(WS(), 0xB2) + r);
			r = blob::Rand73(S16(WS(), 0xAC));
			U16(p, 0x14) = (uint16_t)(U16(WS(), 0xA4) + r);
			r = blob::Rand73(S16(WS(), 0xBC));
			U16(p, 0x16) = (uint16_t)(U16(WS(), 0xB4) + r);
			r = blob::Rand73(S16(WS(), 0xC8));
			U16(p, 0x1E) = (uint16_t)(U16(WS(), 0xC0) + r);
			r = blob::Rand73(S16(WS(), 0xCA));
			U16(p, 0x20) = (uint16_t)(U16(WS(), 0xC2) + r);
			r = blob::Rand73(S16(WS(), 0xCC));
			U16(p, 0x22) = (uint16_t)(U16(WS(), 0xC4) + r);
			S32(p, 0) = mul32(S32(WS(), 0xF4), S32(p, 0)) / 256;
			S32(p, 4) = mul32(S32(WS(), 0xF8), S32(p, 4)) / 256;
			S32(p, 8) = mul32(S32(WS(), 0xFC), S32(p, 8)) / 256;
			U8(p, 0x2C) = U8(desc, 6);
		}
		// 30 fps layer: see mag075_hells_judgement_held.inc
		FX_HELD(held75_note(blk);)
		h_B27130();
		// project the mesh vertices with the parent matrix into the arena top (SXY, SZ3)
		uint8_t *out = PTR(CTX(), 0x74);                               // ebx
		const uint8_t *v = mesh + U32(mesh, 0x14);                     // esi
		for (int32_t k = S32(mesh, 0x18); k > 0; k--)
		{
			x::GteWriteData((int32_t)(((uint32_t)U16(v, 2) << 16) | U16(v, 0)), 0);
			x::GteWriteData((int32_t)U16(v, 4), 1);
			x::GteRTPS();
			x::GteReadSXY2(out);
			U16(out, 4) = MEM<uint16_t>(0x1CA8A5C);                      // SZ3
			v += 8;
			out += 8;
		}
		uint8_t *lst = mesh + U32(mesh, 8);                            // ecx
		uint8_t *verts = PTR(CTX(), 0x74);                             // ebx
		p = PTR(blk, 4);                                               // eax
		int32_t type = S16(lst, 0);
		if (type == 0xFFFF) return;
		for (;;)
		{
			if (type == 8)
			{
				PTR(WS(), 0xF0) = lst;
				p = ShatterTris(p, verts, mesh);
			}
			else if (type == 0x12)
			{
				PTR(WS(), 0xF0) = lst;
				p = ShatterQuads(p, verts, mesh);
			}
			else
				return;
			lst = PTR(WS(), 0xF0);
			type = S16(lst, 0);
			if (type == 0xFFFF) return;
		}
	}

	// draw 61 (MeshRadialLightWave) is the only Eden handler Hell's Judgement carries: take that
	// one slot from apply_eden_b and keep every other slot as init_clone built it
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_draw61(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_eden_b(c);
		VoidFn d61 = c.draw[61];
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[61] = d61;
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_hj);
	}
}
}

	void register_mag075_hells_judgement()
	{
		using namespace gfc;
		hells_judgement::describe(hells_judgement::g_hj);
		init_clone(hells_judgement::g_hj, nullptr, 0);
		hells_judgement::apply_draw61(hells_judgement::g_hj);
		hells_judgement::g_hj.draw[66] = hells_judgement::dh_66_ShatterMesh;   // 0xB429A0
		register_port(0xB3EF50, (void *)hells_judgement::SequenceTask, "MAG075 Hell's Judgement SequenceTick", 75);
		// 30 fps layer: see mag075_hells_judgement_held.inc
		FX_HELD(register_mag075_hells_judgement_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag075_hells_judgement_held.inc"
#endif
