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

// Effect 240: Earthquake (enemy attack 193 of kernel.bin, used by Ochu c0m045; no other kernel.bin
// entry uses effect 240), cinematic-engine module MAG_240 0xA0B330..0xA15A00, script data
// magic/mag239_b.00/.01 (loaded by MAG_240_UNKNOWN_FL 0xA0B330 into 0x2796898 / 0x279689C).
// (Not the Armadodo Earthquake, effect 233 mag233_earthquake.cpp.)
//
// One more compile of the GF cinematic engine (gfc_engine.cpp) with its OWN table layout: the
// bone-side handler block holds two extra 10-entry tables between the prim sizes and the sprite
// orientations (bone_table = draw_table - 0x23C instead of - 0x1EC):
//   ptr_block 0x1863E20, bone 0x1863E48, prim 0x1863E90, prim sizes 0x1863F80,
//   piece-draw table 0x1863F94 (10, only [9]), piece-init table 0x1863FBC (10, only [9]),
//   sprite 0x1863FE4, particle type 0x1864010, particle VM 0x1864024, draw 0x1864084,
//   attribute tables / VM table at the usual draw_table + 0x16A .. + 0x1A4, desc 0x1863D68.
// Instruction-level comparison (module-relative addresses normalised, table references
// symbolic, callee trees hashed) against Ifrit and every ported clone:
//   - SequenceTick 0xA0B9D0 = Ochu Dance's / Breath's (unlit BuildMatricesAndDraw),
//     lit_dispatcher false; BindContext 0xA0B410 and the section set-up 0xA0B4B0 (= Hell's
//     Judgement's 0xB3EA30) are clones;
//   - every VM / bone / prim / sprite / particle handler is the generic (Ifrit) one or a `ret`
//     stub (VM 0x093, prim 7 / 12 / 17 / 19, draw 21 are stubs here); VM 0x006 LoadBattleFile
//     0xA11380 has its own load callback 0xA114B0 (c.load_cb);
//   - the draw handlers are generic (1, 2, 3, 4, 5, 6, 7, 9, 27) except two that exist in no other
//     module (Ifrit's 33 / 34 are stubs), ported below:
//       draw 33 0xA0E800 ShatterSpin (+ piece init 0xA0EFA0 / piece draw 0xA0EB50),
//       draw 34 0xA0E3D0 MeshOnNode.
// The effect's queue (0x27968A0) holds one task, the engine tick 0xA0B9D0.
// The module never writes a battle entity (position +0x1C..+0x20, flags) and has no camera code of
// its own; the battle camera words are written only by the engine (VM 0x039, gfc_engine.cpp).

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag240_ochu_earthquake_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace ochu_quake
{
	static Clone g_c = {};

	static void describe(Clone &c)
	{
		c.name = "240 Ochu Earthquake";
		c.effect_id = 240;
		c.vm_table = 0x1864228;     // VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1863E48;   // BoneHandlerTable (draw_table - 0x23C: two extra tables below)
		c.prim_table = 0x1863E90;   // bone_table + 0x48
		c.prim_size = 0x1863F80;    // bone_table + 0x138
		c.spr_table = 0x1863FE4;    // bone_table + 0x19C (after the two piece tables)
		c.ptype_table = 0x1864010;  // spr_table + 0x2C
		c.pop_table = 0x1864024;    // spr_table + 0x40
		c.draw_table = 0x1864084;   // DrawHandlerTable
		c.attr_16A = 0x18641EE;
		c.attr_184 = 0x1864208;
		c.attr_186 = 0x186420A;
		c.attr_194 = 0x1864218;
		c.attr_19C = 0x1864220;
		c.desc = 0x1863D68;         // draw_table - 0x31C (read by 0xA0B4B0)
		c.ptr_block = 0x1863E20;    // bone_table - 0x28 (read by 0xA0B410)
		c.queue = 0x27968A0;
		c.file00 = 0x2796898;
		c.file01 = 0x279689C;
		c.tick = 0xA0B9D0;
		c.load_cb = 0xA114B0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// the two piece tables of the bone-side handler block (indexed by the prim type of the mesh's
	// first prim group; only type 9, the textured Gouraud triangle, has handlers)
	static const uint32_t PIECE_DRAW_TABLE = 0x1863F94, PIECE_INIT_TABLE = 0x1863FBC;
	static inline VoidFn *PieceDraw() { return g_c.bigtab + (PIECE_DRAW_TABLE - 0x1863E48) / 4; }
	static inline VoidFn *PieceInit() { return g_c.bigtab + (PIECE_INIT_TABLE - 0x1863E48) / 4; }

	// external helpers (original addresses)
	// 0xB65150 xorEAX_6: `xor eax, eax; ret`, result unused (call-trace parity)
	static inline void xl_Nop_B65150() { blob::f<int32_t (__cdecl *)()>(0xB65150)(); }
	// 0x56C880 TransformWorldCoordinateToProjectedSpace(v, sxy, p, flag): RTPS of v, SXY2 -> sxy,
	// returns OTZ
	static inline int32_t xl_RotTransPers(const void *v, void *sxy, void *p, void *flag) { return x::f<int32_t (__cdecl *)(const void *, void *, void *, void *)>(0x56C880)(v, sxy, p, flag); }
	// 0x56CA30 NormalColorCol(normal, colour, out): GTE colour of a normal (light / colour matrix
	// of LightColourSetup) -> out
	static inline void xl_NormalColor_56CA30(const void *n, const void *col, void *out) { x::f<void (__cdecl *)(const void *, const void *, void *)>(0x56CA30)(n, col, out); }
	// 0x45E020: GTE V0..V2 from three consecutive SVECTORs
	static inline void xl_GteLoadV012Block(const void *v) { x::f<void (__cdecl *)(const void *)>(0x45E020)(v); }
	// 0x45C7A0 SSIGPU_InsertPrimAutoDepth(ot, prim)
	static inline void xl_InsertPrimAutoDepth(uint32_t ot, void *prim)
	{
		// 30 fps layer: see mag240_ochu_earthquake_held.inc
		FX_HELD(if (held_predicting()) return;)
		x::f<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(ot, prim);
	}
	// GTE data register 7 (OTZ), read directly by the module
	static inline int32_t GteOTZ() { return MEM<int32_t>(0x1CA8A2C); }

	static inline int16_t Div3(int32_t s) { return (int16_t)(s / 3); }       // imul 0x55555556: truncating

	// ------------------------------------------------------------------------------------
	// 0xA0EFA0 (piece init [9], first draw of draw 33): for each record of the first prim group
	// (GT3, 0x1C bytes; u16 vertex offsets +0x12 / +0x14 / +0x16 into the UNSCALED mesh vertices
	// ws+0xF4) the centroid -> piece +0 / +2 / +4 (16-byte pieces from ws+0xF8). ws+0xF0 = the
	// group's header on entry, the record after the last on exit; ws+0xF8 = the piece after the last.
	// ------------------------------------------------------------------------------------
	static void __cdecl pi_09_PieceCentroids()
	{
		uint8_t *ws = WS();
		uint8_t *hdr = PTR(ws, 0xF0);
		uint8_t *verts = PTR(ws, 0xF4);                   // [ebp-0xc]
		uint8_t *pc = PTR(ws, 0xF8);                      // ebx
		int32_t n = S16(hdr, 2);
		uint8_t *rec = hdr + 4;                           // [ebp-4]
		if (n <= 0)
		{
			PTR(ws, 0xF0) = rec;
			PTR(WS(), 0xF8) = pc;
			return;
		}
		do
		{
			uint8_t *v1 = verts + U16(rec, 0x14);         // esi
			uint8_t *v2 = verts + U16(rec, 0x16);         // edi
			uint8_t *v0 = verts + U16(rec, 0x12);         // ecx
			pc += 0x10;
			S16(pc, -0x10) = Div3(add32(add32(S16(v2, 0), S16(v1, 0)), S16(v0, 0)));
			S16(pc, -0x0E) = Div3(add32(add32(S16(v2, 2), S16(v1, 2)), S16(v0, 2)));
			S16(pc, -0x0C) = Div3(add32(add32(S16(v2, 4), S16(v1, 4)), S16(v0, 4)));
			rec += 0x1C;
		} while (--n != 0);
		PTR(WS(), 0xF0) = rec;
		PTR(WS(), 0xF8) = pc;
	}

	// ------------------------------------------------------------------------------------
	// 0xA0EB50 (piece draw [9]): every GT3 record of the first prim group drawn as a free piece.
	// Per record (ws+0xD0 = group header, ws+0xD4 = scaled vertices, ws+0xD8 = pieces):
	//   spin angles ws+0xB0..0xB4 = (s16)(ws+0x5C * piece +8 / +0xA / +0xC) (16-bit products);
	//   centroid c ws+0xA8..0xAC of the three scaled vertices (/3 truncating);
	//   piece position ws+0xB8..0xBC = c * ws+0x58 / 256 (C division) + ws+0xA0..0xA4;
	//   GTE TR = parent * position, R = rotation(angles) * parent rotation (0x56CD50 / 0x56C220);
	//   packet (POLY_GT3, 0x28 B at ws+0x60): code 0x34 | semi-trans (bone+0xCC bit 25), tpage
	//   ws+0x50, clut rec+0x18 + ws+0x54, uv rec+0xC / +0xE / +0x10, colours 0x56CA30 of
	//   rec+0 / +4 / +8; vertices - c through RTPT, OT slot RT+0x4C + (OTZ >> 2) * 4.
	// Exit: ws+0x60 = next packet, ws+0xF0 = the record after the last, ws+0xF8 = next piece.
	// ------------------------------------------------------------------------------------
	static void __cdecl pd_09_DrawPiecesGT3()
	{
		uint8_t *ws = WS();
		uint8_t *verts = PTR(ws, 0xD4);                   // [ebp-0x18]
		uint8_t *hdr = PTR(ws, 0xD0);
		uint8_t *pc = PTR(ws, 0xD8);                      // ebx / [ebp-8]
		uint8_t *pkt = PTR(ws, 0x60);                     // esi
		const uint8_t code = (uint8_t)((int32_t)((U32(CUR(), 0xCC) & 0x2000000u) | 0x34000000u) >> 24); // [ebp-0x14]
		uint8_t *rec = hdr + 4;                           // [ebp-4]
		int32_t n = S16(hdr, 2);                          // [ebp-0x10]
		if (n > 0)
		{
			do
			{
				uint8_t *r4 = rec + 4;                    // edi
				x::SetRotMatrix(PTR(WS(), 0x38));
				x::SetTransVector(PTR(WS(), 0x38));
				ws = WS();
				U16(ws, 0xB0) = (uint16_t)((uint32_t)U16(ws, 0x5C) * U16(pc, 8));
				U16(WS(), 0xB2) = (uint16_t)((uint32_t)U16(WS(), 0x5C) * U16(pc, 0xA));
				U16(WS(), 0xB4) = (uint16_t)((uint32_t)U16(WS(), 0x5C) * U16(pc, 0xC));
				ws = WS();
				uint8_t *vb = PTR(ws, 0x7C);
				uint8_t *v0 = vb + U16(r4, 0xE);           // ecx
				uint8_t *v1 = vb + U16(r4, 0x10);          // ebx
				uint8_t *v2 = vb + U16(r4, 0x12);          // [ebp-0xc]
				S16(WS(), 0xA8) = Div3(add32(add32(S16(v2, 0), S16(v1, 0)), S16(v0, 0)));
				S16(WS(), 0xAA) = Div3(add32(add32(S16(v2, 2), S16(v1, 2)), S16(v0, 2)));
				S16(WS(), 0xAC) = Div3(add32(add32(S16(v2, 4), S16(v1, 4)), S16(v0, 4)));
				ws = WS();
				U16(ws, 0xB8) = (uint16_t)(mul32(S16(ws, 0xA8), S32(ws, 0x58)) / 256 + U16(ws, 0xA0));
				U16(WS(), 0xBA) = (uint16_t)(mul32(S16(WS(), 0xAA), S32(WS(), 0x58)) / 256 + U16(WS(), 0xA2));
				U16(WS(), 0xBC) = (uint16_t)(mul32(S16(WS(), 0xAC), S32(WS(), 0x58)) / 256 + U16(WS(), 0xA4));
				x::GteLoadV0(WS() + 0xB8);
				x::GteMVMVA_RotV0Tr();
				x::Gte_45E580();
				x::RotMatrixFromAngles(WS() + 0xB0, WS() + 0xC0);
				x::MulMatrixInPlace(WS() + 0xC0, PTR(WS(), 0x38));
				// (the product's never-written pad word +0x12 = ws+0xD2 is overwritten below)
				x::SetRotMatrix(WS() + 0xC0);
				U8(pkt, 3) = 9;
				U16(pkt, 0x1A) = U16(WS(), 0x50);
				U16(pkt, 0xE) = (uint16_t)(U16(r4, 0x14) + U16(WS(), 0x54));
				xl_NormalColor_56CA30(WS() + 0xE0, rec, pkt + 4);
				xl_NormalColor_56CA30(WS() + 0xE0, r4, pkt + 0x10);
				xl_NormalColor_56CA30(WS() + 0xE0, rec + 8, pkt + 0x1C);
				U8(pkt, 7) = code;
				U16(pkt, 0xC) = U16(r4, 8);
				U16(pkt, 0x18) = U16(r4, 0xA);
				U16(pkt, 0x24) = U16(r4, 0xC);
				// the vertices relative to the centroid (16-bit differences) -> ws+0xC0 / 0xC8 / 0xD0
				for (int k = 0; k < 3; k++)
				{
					uint8_t *v = verts + U16(r4, 0xE + 2 * k);
					uint8_t *o = WS() + 0xC0 + 8 * k;
					U16(o, 0) = (uint16_t)(U16(v, 0) - U16(WS(), 0xA8));
					U16(WS() + 0xC0 + 8 * k, 2) = (uint16_t)(U16(v, 2) - U16(WS(), 0xAA));
					U16(WS() + 0xC0 + 8 * k, 4) = (uint16_t)(U16(v, 4) - U16(WS(), 0xAC));
				}
				xl_GteLoadV012Block(WS() + 0xC0);
				x::GteRTPT();
				x::GteReadSXY012(pkt + 8, pkt + 0x14, pkt + 0x20);
				x::GteAVSZ3();
				xl_InsertPrimAutoDepth(U32(RT(), 0x4C) + (uint32_t)(GteOTZ() >> 2) * 4u, pkt);
				pkt += 0x28;
				rec += 0x1C;
				pc += 0x10;
			} while (--n != 0);
		}
		ws = WS();
		PTR(ws, 0x60) = pkt;
		PTR(WS(), 0xF0) = rec;
		PTR(WS(), 0xF8) = pc;
	}

	// ------------------------------------------------------------------------------------
	// 0xA0E800 (Draw 33 ShatterSpin): a mesh (object desc+0, desc = bone+0xB8) broken into its
	// triangles, each turning about its own centroid and pushed out from the mesh centre.
	// First draw (bone+0xBC == 0): mesh -> bone+0xD8; arena block (first group count + 1) * 16 ->
	// bone+0xBC (16 header bytes, then one 16-byte piece per record); piece init table[type]
	// (centroids); per piece three 0xB65740 random spins in [-desc+4, desc+4] -> +8 / +0xA / +0xC.
	// Every draw: ws+0x50 tpage = bone+0x92, ws+0x54 clut add = bone+0x9A, ws+0x58 push-out =
	// outAngle.x (bone+0x8C, /256), ws+0x5C spin = outAngle.y (+0x8E), ws+0x8C mesh scale =
	// outAngle.z (+0x90, /256), ws+0xA0..0xA4 = outPos; vertices scaled into the arena; colour
	// setup 0xA0E2B0 (= Ifrit's 0xB29450) of bone+0xCC; piece draw table[type].
	// The pieces are fixed after the first draw (the draw does not depend on the freeze flag).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_33_ShatterSpin()
	{
		if (U32(CUR(), 0xBC) == 0)
		{
			uint8_t *m = blob::ObjectPtr(S16(PTR(CUR(), 0xB8), 0));
			PTR(CUR(), 0xD8) = m;
			uint8_t *mesh = PTR(CUR(), 0xD8);                                  // esi
			uint8_t *hdr = mesh + U32(mesh, 8);
			PTR(WS(), 0x6C) = hdr;
			uint8_t *blk = blob::ArenaAlloc(shl32(add32(S16(hdr, 2), 1), 4));
			PTR(CUR(), 0xBC) = blk;
			U32(WS(), 0x90) = U32(CUR(), 0xBC);
			U32(WS(), 0x94) = U32(WS(), 0x90) + 0x10;
			U32(WS(), 0x74) = U32(mesh, 0x14) + (uint32_t)mesh;
			U32(WS(), 0xF0) = U32(WS(), 0x6C);
			uint8_t *h = PTR(WS(), 0xF0);                                      // esi
			U32(WS(), 0xF4) = U32(WS(), 0x74);
			U32(WS(), 0xF8) = U32(WS(), 0x94);
			PieceInit()[S16(h, 0)]();
			int32_t range = S16(PTR(CUR(), 0xB8), 4);                           // edi
			uint8_t *pc = PTR(WS(), 0x94);
			int32_t n = S16(h, 2);
			if (n > 0)
			{
				uint8_t *q = pc + 0xA;
				do
				{
					S16(q, -2) = (int16_t)blob::Rand73(range);
					S16(q, 0) = (int16_t)blob::Rand73(range);
					S16(q, 2) = (int16_t)blob::Rand73(range);
					q += 0x10;
				} while (--n != 0);
			}
		}
		U32(WS(), 0x90) = U32(CUR(), 0xBC);
		U32(WS(), 0x94) = U32(WS(), 0x90) + 0x10;
		S32(WS(), 0x50) = S16(CUR(), 0x92);
		S32(WS(), 0x54) = S16(CUR(), 0x9A);
		S32(WS(), 0x58) = S16(CUR(), 0x8C);
		S32(WS(), 0x5C) = S16(CUR(), 0x8E);
		S32(WS(), 0x8C) = S16(CUR(), 0x90);
		U32(WS(), 0xA0) = U32(CUR(), 0x94);
		U16(WS(), 0xA4) = U16(CUR(), 0x98);
		PTR(WS(), 0x38) = (uint8_t *)blob::GetParentMatrix((int32_t)U16(CUR(), 0x9C));
		uint8_t *mesh = PTR(CUR(), 0xD8);
		U32(WS(), 0x6C) = U32(mesh, 8) + (uint32_t)mesh;
		U32(WS(), 0x74) = U32(mesh, 0x14) + (uint32_t)mesh;
		U32(WS(), 0x7C) = U32(CTX(), 0x74);
		int32_t n = S32(mesh, 0x18);
		uint8_t *s = PTR(WS(), 0x74);                                          // esi
		uint8_t *d = PTR(WS(), 0x7C);                                          // ecx
		if (n > 0)
		{
			do
			{
				d += 8;
				s += 8;
				S16(d, -8) = (int16_t)(mul32(S32(WS(), 0x8C), S16(s, -8)) / 256);
				S16(d, -6) = (int16_t)(mul32(S16(s, -6), S32(WS(), 0x8C)) / 256);
				S16(d, -4) = (int16_t)(mul32(S16(s, -4), S32(WS(), 0x8C)) / 256);
			} while (--n != 0);
		}
		h_B29450((int32_t)U32(CUR(), 0xCC));
		U32(WS(), 0x60) = U32(CTX(), 0x7C);
		U32(WS(), 0xD0) = U32(WS(), 0x6C);
		U32(WS(), 0xD4) = U32(WS(), 0x7C);
		uint8_t *h = PTR(WS(), 0xD0);
		U32(WS(), 0xD8) = U32(WS(), 0x94);
		PieceDraw()[S16(h, 0)]();
		U32(CTX(), 0x7C) = U32(WS(), 0x60);
	}

	// ------------------------------------------------------------------------------------
	// 0xA0E3D0 (Draw 34 MeshOnNode): mesh (object desc+2, desc = bone+0xB8 {s16 flags / order
	// (+0, bit 3 = clamp, >> 8 = rotation order), s16 object, s16 max Y, s16 node ref}) placed in
	// the frame of another node: R = rotation order(node angles) scaled by bone+0x8C..0x90 << 4,
	// TR = R * outPos + node position. First draw (bone+0xBC == 0): bone+0xD8 = the object,
	// bone+0xBC = desc+0 (the flags word, as a "loaded" marker). Vertices to world space
	// (0x56C820, 16-byte MAC records after the arena copy) then, flag bit 3 set, copied into the
	// arena with Y clamped to <= max Y (flag clear: the arena keeps what it held); projected with
	// the camera node (0x56C880, SXY / OTZ in place); the prim stream through C().prim[type] (no
	// +20 / +40 variants). ws+0x94 = desc pointer (the prim renderers' depth bias slot).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_34_MeshOnNode()
	{
		uint8_t *cur = CUR();
		if (U32(cur, 0xBC) == 0)
		{
			uint8_t *dsc = PTR(cur, 0xB8);                                     // esi
			PTR(CUR(), 0xD8) = blob::ObjectPtr(S16(dsc, 2));
			S32(CUR(), 0xBC) = S16(dsc, 0);
			cur = CUR();
		}
		U32(WS(), 0x94) = U32(cur, 0xB8);
		U32(WS(), 0x60) = U32(CTX(), 0x7C);
		U32(WS(), 0x50) = (uint32_t)(int32_t)S16(CUR(), 0x92) | 0xE1000000u;
		S32(WS(), 0x54) = S16(CUR(), 0x9A);
		U32(WS(), 0x84) = U32(CUR(), 0xCC) & 0x2FFFFFFu;
		U32(WS(), 0x8C) = U32(WS(), 0x84) & 0x2000000u;
		U32(WS(), 0x5C) = U32(RT(), 0x4C);
		U32(WS(), 0x90) = U8(CUR(), 0xDE);
		U32(WS(), 0x98) = U16(CUR(), 0x9E);
		U32(WS(), 0x68) = U32(CUR(), 0xD8);
		uint8_t *mesh = PTR(WS(), 0x68);                                       // esi
		U32(WS(), 0x6C) = U32(mesh, 8) + (uint32_t)mesh;
		U32(WS(), 0x74) = U32(mesh, 0x14) + (uint32_t)mesh;
		U32(WS(), 0x7C) = U32(CTX(), 0x74);
		U32(WS(), 0x80) = U32(WS(), 0x7C) + U32(mesh, 0x18) * 8u;
		uint8_t *dsc = PTR(WS(), 0x94);
		S32(WS(), 0x78) = S16(dsc, 0);
		uint8_t *node = blob::GetBone(S16(dsc, 6));                            // edi
		xl_Nop_B65150();
		Mat4x3 *r = blob::RotMatrixOrder(node + 0x8C, S32(WS(), 0x78) >> 8);    // ebx
		S32(WS(), 0xF4) = shl32(S16(CUR(), 0x8C), 4);
		S32(WS(), 0xF8) = shl32(S16(CUR(), 0x8E), 4);
		S32(WS(), 0xFC) = shl32(S16(CUR(), 0x90), 4);
		x::ScaleMatrix(r, WS() + 0xF4);
		x::SetRotMatrix(r);
		int32_t px = S16(node, 0x94), pz = S16(node, 0x98), py = S16(node, 0x96);
		x::GteWriteCtrl(px, 5);
		x::GteWriteCtrl(py, 6);
		x::GteWriteCtrl(pz, 7);
		x::GteLoadV0(CUR() + 0x94);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
		uint8_t *ws = WS();
		int32_t n = S32(mesh, 0x18);
		uint8_t *v = PTR(ws, 0x74);                                            // edi
		uint8_t *mac = PTR(ws, 0x80);                                          // ebx
		if (n > 0)
		{
			do
			{
				x::TransformToCamera(v, mac, WS() + 0xFC);
				v += 8;
				mac += 0x10;
			} while (--n != 0);
		}
		ws = WS();
		uint8_t *a = PTR(ws, 0x7C);                                            // ecx
		int32_t lim = S16(PTR(ws, 0x94), 4);                                   // edi
		uint8_t *s = PTR(ws, 0x80);                                            // edx
		int32_t cnt = S32(mesh, 0x18);                                         // ebx
		if ((U8(ws, 0x78) & 8) && cnt > 0)
		{
			do
			{
				U16(a, 0) = U16(s, 0);
				U16(a, 4) = U16(s, 8);
				int32_t y = S16(s, 4);
				if (y > lim) y = lim;
				U16(a, 2) = (uint16_t)y;
				a += 8;
				s += 0x10;
			} while (--cnt != 0);
		}
		x::SetRotMatrix(NODEMAT());
		x::SetTransVector(NODEMAT());
		uint8_t *w = WS();                                                     // edx
		int32_t k = S32(mesh, 0x18);
		uint8_t *p = PTR(w, 0x7C);
		if (k > 0)
		{
			do
			{
				int32_t z = xl_RotTransPers(p, p, WS() + 0xFC, WS() + 0xFC);
				S32(p, 4) = z;
				w = WS();
				p += 8;
			} while (--k != 0);
		}
		// prim stream {s16 type, s16 count} + count records of prim_size[type]
		uint8_t *hdr = PTR(w, 0x6C);
		int32_t c = S16(hdr, 2);
		while (c != -1)
		{
			S32(w, 0x70) = c;
			U32(WS(), 0x6C) = U32(WS(), 0x6C) + 4;
			int32_t type = S16(hdr, 0);
			uint8_t *w2 = WS();
			uint32_t size = MEM<uint8_t>(C().prim_size + (uint32_t)type);
			U32(w2, 0x58) = (uint32_t)mul32((int32_t)size, S32(w2, 0x70)) + U32(w2, 0x6C);
			C().prim[type]();
			U32(WS(), 0x6C) = U32(WS(), 0x58);
			w = WS();
			hdr = PTR(w, 0x6C);
			c = S16(hdr, 2);
		}
		U32(CTX(), 0x7C) = U32(w, 0x60);
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag240_ochu_earthquake()
	{
		using namespace gfc;
		ochu_quake::describe(ochu_quake::g_c);
		init_clone(ochu_quake::g_c, nullptr, 0);
		ochu_quake::g_c.draw[33] = ochu_quake::dh_33_ShatterSpin;                 // 0xA0E800
		ochu_quake::g_c.draw[34] = ochu_quake::dh_34_MeshOnNode;                  // 0xA0E3D0
		ochu_quake::PieceDraw()[9] = ochu_quake::pd_09_DrawPiecesGT3;            // 0xA0EB50
		ochu_quake::PieceInit()[9] = ochu_quake::pi_09_PieceCentroids;           // 0xA0EFA0
		register_port(0xA0B9D0, (void *)ochu_quake::SequenceTask, "MAG240 Ochu Earthquake SequenceTick", 240);
		// 30 fps layer: see mag240_ochu_earthquake_held.inc
		FX_HELD(register_mag240_ochu_earthquake_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag240_ochu_earthquake_held.inc"
#endif
